#include <types.h>
#include <current.h>
#include <proc.h>
#include <lib.h>
#include <sys_functions.h>
#include <spl.h>
#include <thread.h>
#include <kern/fcntl.h>
#include <vnode.h>
#include <uio.h>
#include <vfs.h>
#include <copyinout.h>
#include <synch.h>
#include <kern/errno.h>
#include <thread.h>
#include <kern/unistd.h>

/**
 * All non-file related syscall functions for A2 goes here
 *
 * Functions:
 *
 * sys__exit
 * sys_write
 * sys_open
 */

void sys__exit(int exitcode) {
	int dbflags = DB_A2;

	(void) exitcode;
	struct proc *process = curthread->t_proc;

	KASSERT(process != NULL);

	int processCount = threadarray_num(&process->p_threads);

	//turn off all interrupts
	int original_spl = splhigh();

	//detach all threads used by this process
	for (int i = 0; i < processCount; i++) {
		threadarray_remove(&process->p_threads, i);
	}
	KASSERT(threadarray_num(&process->p_threads) == 0);

	//destroy process
	proc_destroy(process);

	DEBUG(DB_A2, "HERE");

	//turn spl back to original level
	splx(original_spl);

	thread_exit();

}
/**
 * Return value: nbytes read is returned.
 * This count should be positive.
 * A return value of 0 should be construed as signifying end-of-file.
 * On error, read returns -1 and sets errno to a suitable error code for the error condition encountered.
 */
int sys_read(int fd, void* buf, size_t nbytes, int32_t *retval) {

	int dbflags = DB_A2;
	int readBytes = 0;
	struct file_desc* file;
	struct iovec iov;
	struct uio uio_R; //uio for reading

	DEBUG(DB_A2, "buf is %d at %p\n", *((int* )buf), buf);

	//if invalid fd
	if (fd < 0 || fd > MAX_OPEN_COUNT - 1) {
		return EBADF;
	}

	//check if file is in fd_table
	if (curthread->t_proc->fd_table[fd] == NULL) {
		return EBADF;
	}

	file = curthread->t_proc->fd_table[fd];

	//if file is write_only
	if (file->f_flags == O_WRONLY) {
		return EBADF;
	}

	int old_spl = splhigh();
	lock_acquire(file->f_lock);

	//setup uio to read
	uio_kinit(&iov, &uio_R, buf, nbytes, file->f_offset, UIO_READ);

	if (VOP_READ(file->f_vn, &uio_R)) {
		return -1;
	}

	readBytes = nbytes - uio_R.uio_resid;
	file->f_offset = uio_R.uio_offset;

	lock_release(file->f_lock);
	splx(old_spl);

	DEBUG(DB_A2, "buf is %d at %p\n", *((int* )buf), buf);
	DEBUG(DB_A2, "readBytes is %d\n", readBytes);

	*retval = readBytes;

	KASSERT(curthread->t_curspl == 0);
	return 0;
}

/**
 *  The count of bytes written is returned. This count should be positive.
 *  A return value of 0 means that nothing could be written, but that no error occurred;
 *  this only occurs at end-of-file on fixed-size objects. On error, write returns -1 and
 *  sets errno to a suitable error code for the error condition encountered.
 *  Note that in some cases, particularly on devices, fewer than buflen (but greater than zero) bytes may be written.
 *  This depends on circumstances and does not necessarily signify end-of-file.
 *  In most cases, one should loop to make sure that all output has actually been written.
 */
int sys_write(int fd, const void *buf, size_t nbytes, int32_t *retval) {
	int dbflags = DB_A2;
	int wroteBytes = 0;
	struct file_desc* file = NULL; //file
	struct iovec iov;
	struct uio uio_W; //uio for writing

	switch (fd) {
	case STDOUT_FILENO: //stdout
		wroteBytes = kprintf(buf);

		break;
	case STDIN_FILENO: //stdin
		break;
	case STDERR_FILENO:
		break;
	default:

		//if invalid fd
		if (fd < 0 || fd > MAX_OPEN_COUNT - 1) {
			return EBADF;
		}

		//check if the file is even the table
		if (curthread->t_proc->fd_table[fd] == NULL) {
			DEBUG(DB_A2, "error EBADF\n");
			return EBADF;
		}
		file = curthread->t_proc->fd_table[fd];

		//if flag is read only
		if (file->f_flags == O_RDONLY) {
			return EBADF;
		}

		lock_acquire(file->f_lock);

		KASSERT(file->f_lock != NULL);

		//setup uio to write to file
		uio_kinit(&iov, &uio_W, (void *) buf, nbytes, file->f_offset,
				UIO_WRITE);

		KASSERT(file->f_vn->vn_opencount >= 1);
		KASSERT(file->f_vn->vn_refcount >= 1);

		//writing....
		if (VOP_WRITE(file->f_vn, &uio_W)) {
			DEBUG(DB_A2, "error in VOP_WRITE\n");
			return -1;
		}

		//set the bytes written
		wroteBytes = nbytes - uio_W.uio_resid;
		//update offset
		file->f_offset = uio_W.uio_offset;
		lock_release(file->f_lock);
		break;
	}

	*retval = wroteBytes;
	KASSERT(curthread->t_curspl == 0);

	return 0;
}

int sys_open(const char *filename, int flags, int mode, int32_t *retval) {
	int dbflags = DB_A2;
	int fd = 0; //filehandle to be returned
	bool opened = false;
	struct file_desc** table = curthread->t_proc->fd_table;
	int openedFileIndex = -1;

	//check filename
	if (strlen(filename) > MAX_LEN_FILENAME) {
		return EFAULT;
	}

	//if file is the console: stdin, stdout, stderr
	if (filename == "con:" && flags == O_RDONLY) {
		fd = STDIN_FILENO;	//0
	} else if (filename == "con:" && flags == O_WRONLY && mode == 0664) {
		fd = STDOUT_FILENO; //1
	} else if (filename == "con:" && flags == O_WRONLY && mode == 0665) {
		fd = STDERR_FILENO; //2
	} else {

		//check in fd_table to see if file of the same name is already open
		KASSERT(MAX_OPEN_COUNT > 4);
		for (int i = 3; i < MAX_OPEN_COUNT; i++) {

			if (table[i] == NULL) {
				break;
			}
			if (table[i]->f_name == filename) {
				opened = true;
				openedFileIndex = i; //capture the vnode ( we don't want to duplicate the same vnode)
			}
		}
		fd = 3;
		while (fd < MAX_OPEN_COUNT && table[fd] != NULL) {
			if (table[fd]->available == true) {
				break;
			}
			fd++;
		}
		DEBUG(DB_A2, "file will be opened with fd = %d\n", fd);

		//if there is room in table to add a fd entry
		if (fd >= MAX_OPEN_COUNT) {
			return EMFILE;
		} else {

			struct vnode* openFile = (openedFileIndex >= 0) ? table[openedFileIndex]->f_vn : NULL;

			//open file and add entry to fdtable
			if (!opened && openFile == NULL) {
				DEBUG(DB_A2, "HERE\n");
				KASSERT(openFile == NULL);
				if (vfs_open((char*) filename, flags, mode, &openFile)) {
					//if there is a problem opening the file
					return ENOENT;
				}
			} else {

				KASSERT(openFile != NULL);
				KASSERT(openFile->vn_opencount >= 1);
				KASSERT(openFile->vn_refcount >= 1);

				vnode_incopen(openFile); //inc open count of the vnode referred to
				vnode_incref(openFile);
			}

			if (table[fd] == NULL)
				table[fd] = kmalloc(sizeof(struct file_desc));
			table[fd]->f_vn = openFile;
			table[fd]->f_name = (char *) filename;
			table[fd]->f_offset = 0;
			table[fd]->f_flags = flags;
			table[fd]->f_lock = (openedFileIndex >= 0)? table[openedFileIndex]->f_lock : lock_create(filename);
			table[fd]->available = false;

			KASSERT(table[fd]->f_vn->vn_opencount >= 1);
			KASSERT(table[fd]->f_vn->vn_refcount >= 1);

			DEBUG(DB_A2, "filename %s fd %d opencount is %d, vn = %p\n",
					table[fd]->f_name, fd, table[fd]->f_vn->vn_opencount,
					table[fd]->f_vn);
			DEBUG(DB_A2, "filename %s fd %d refcount is %d\n",
					table[fd]->f_name, fd, table[fd]->f_vn->vn_refcount);
		}
	}
	*retval = (int32_t) fd;
	KASSERT(curthread->t_curspl == 0);

	return 0;
}

int sys_close(int fd) {

	//int dbflags = DB_A2;
	//make sure the file is in table
	KASSERT(curthread->t_curspl == 0);

	//if invalid fd
	if (fd < 0 || fd > MAX_OPEN_COUNT - 1) {
		return EBADF;
	}
	//if no entry in the table
	if (curthread->t_proc->fd_table[fd] == NULL) {
		return EBADF;
	}

	KASSERT(curthread->t_proc->fd_table[fd] != NULL);
	struct file_desc* file = curthread->t_proc->fd_table[fd];

	if (file->available == true) {
		return EBADF;
	}

	KASSERT(file->available == false);

	//if this file is not opened
	if (file->f_vn->vn_opencount <= 0) {
		return EIO;
	}

	lock_acquire(file->f_lock);		//maybe use a spinlock here instead

	if (file->f_vn->vn_refcount <= 1) {
		//I am the only file in the table that refers to this vnode
		KASSERT(file->f_vn->vn_opencount == 1);
		vnode_decopen(file->f_vn);
		KASSERT(file->f_vn->vn_opencount == 0);

		vnode_cleanup(file->f_vn);
		struct vnode* temp = file->f_vn;
		file->f_vn = NULL;
		kfree(temp);
	} else if (file->f_vn->vn_refcount > 1) {
		//some other files are still referring to this vnode
		vfs_close(file->f_vn);

		//stop referencing to the vnode
		file->f_vn = NULL;
	}

	//set closed entry to an available entry in the table
	file->available = true;

	lock_release(file->f_lock);

	return 0;
}

pid_t sys_fork() {

	/**
	 * Note: child should be "born" with the same state as the parent
	 *
	 * child's address space = copy of(parent's address space)
	 * return child_pid to parent
	 * return 0 to child_pid
	 * child_pid != parent_pid
	 */

	struct proc* parent = curthread->t_proc;

	//turn off interrupts
	int old_spl = splhigh();

	//create child process (make sure child starts with the same context as parent)
	//best way is to look at parent's current trapframe and go from there

	//copy parent's address space to child

	//create thread in child process

	/**
	 * copy parent's fd_table to child
	 * Note: However, the file handle objects the file tables point to are shared,
	 * so, for instance, calls to lseek in one process can affect the other.
	 */

	//turn interrupts back on
	splx(old_spl);
	KASSERT(curthread->t_curspl == 0);

	//return
	(void) parent;
	return 0;
}
