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
 * sys_read
 * sys_write
 * sys_open
 * sys_close
 *
 */

void sys__exit(int exitcode) {
	(void) exitcode;
	struct proc *process = curthread->t_proc;

	KASSERT(process != NULL);

	int processThreadCount = threadarray_num(&process->p_threads);

	//detach all threads used by this process
	for (int i = 0; i < processThreadCount; i++) {
		threadarray_remove(&process->p_threads, i);
	}
	KASSERT(threadarray_num(&process->p_threads) == 0);

	//destroy process
	KASSERT(process != NULL);
	proc_destroy(process);
	curthread->t_proc = NULL;
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
	int errno = -1;

	//check for valid fd
	if (fd < 0 || fd > MAX_OPEN_COUNT - 1) {
		return EBADF;
	}

	//reading from stdin
	if (fd == STDIN_FILENO) {
		if ((errno = sys_open("con:", O_RDONLY, 0, &fd))) {
			return errno;
		}
	}

	file = curthread->t_proc->fd_table[fd];

	//check if file is opened
	if (file == NULL) {
		return EBADF;
	}

	//check if file is writeonly
	if (file->f_flags == O_WRONLY) {
		return EBADF;
	}

	lock_acquire(file->f_lock);

	//setup uio to read
	uio_kinit(&iov, &uio_R, buf, nbytes, file->f_offset, UIO_READ);

	if (VOP_READ(file->f_vn, &uio_R)) {
		return -1;
	}

	readBytes = nbytes - uio_R.uio_resid;
	file->f_offset = uio_R.uio_offset;

	lock_release(file->f_lock);

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
	int errno = -1;
	struct file_desc* file = NULL; //file
	struct iovec iov;
	struct uio uio_W; //uio for writing

	//check for valid fd
	if (fd < 0 || fd > MAX_OPEN_COUNT - 1) {
		return EBADF;
	}

	switch (fd) {
	case STDOUT_FILENO:
		if ((errno = sys_open("con:", O_WRONLY, 0664, &fd))) {
			DEBUG(DB_A2, "ERROR WRITING TO STDOUT!\n");
			return errno;
		}
		break;
	case STDERR_FILENO:
		if ((errno = sys_open("con:", O_WRONLY, 0665, &fd))) {
			DEBUG(DB_A2, "ERROR WRITING TO STDERR!\n");
			return errno;
		}
		break;
	default:
		break;
	}

	file = curthread->t_proc->fd_table[fd];

	//check if the file is opened
	if (file == NULL) {
		return EBADF;
	}

	//check if file is readonly
	if (file->f_flags == O_RDONLY) {
		return EBADF;
	}

	KASSERT(file->f_lock != NULL);

	lock_acquire(file->f_lock);

	//setup uio to write to file
	uio_kinit(&iov, &uio_W, (void *) buf, nbytes, file->f_offset, UIO_WRITE);

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

	*retval = wroteBytes;
	KASSERT(curthread->t_curspl == 0);

	return 0;
}

int sys_open(const char *filename, int flags, int mode, int32_t *retval) {
	int dbflags = DB_A2;
	int fd = -1; //filehandle to be returned
	struct file_desc** table = curthread->t_proc->fd_table;
	struct vnode* openedVnode = NULL;

	//check if filename is a valid pointer
	if ((uint32_t) filename >= 0xffffffff) {
		DEBUG(DB_A2, "filename is not a valid pointer\n");
		return EFAULT;
	}

	//check if filename is of correct size
	if (strlen(filename) > MAX_LEN_FILENAME || filename == ""
			|| strlen(filename) == 0) {
		DEBUG(DB_A2, "filename is of invalid size (too small/toobig)\n");
		return EBADF;
	}

	//if file is the console: stdin, stdout, stderr
	if (filename == "con:" && flags == O_RDONLY) {
		fd = STDIN_FILENO;	//0
	} else if (filename == "con:" && flags == O_WRONLY && mode == 0664) {
		fd = STDOUT_FILENO; //1
	} else if (filename == "con:" && flags == O_WRONLY && mode == 0665) {
		fd = STDERR_FILENO; //2
	}

	KASSERT(MAX_OPEN_COUNT > 4);

	//check in fd_table to see if file is already open
	if (fd < 3 && fd >= 0) {
		//standard console related
		KASSERT(filename == "con:");
		if (table[fd] != NULL) {
			openedVnode = table[fd]->f_vn;
		}

	} else {
		//file related
		for (int i = 3; i < MAX_OPEN_COUNT; i++) {

			if (table[i] == NULL) {
				break;
			}
			if (table[i]->f_name == filename) {
				openedVnode = table[i]->f_vn;
			}
		}
		//check if we reached MAX_OPEN_COUNT
		fd = 3;
		while (fd < MAX_OPEN_COUNT && table[fd] != NULL) {
			fd++;
		}
	}

	//check to see if the row we found is valid
	if (fd >= MAX_OPEN_COUNT) {
		return EMFILE;
	}

	//config vnode: a new one or reference to an existing
	struct vnode* openFile = (openedVnode == NULL) ? NULL : openedVnode;

	if (openFile == NULL) {
		//open file and add entry to fdtable
		char * filename_W = kstrdup(filename);
		if (vfs_open(filename_W, flags, mode, &openFile)) {
			//if there is a problem opening the file
			return ENOENT;
		}
		kfree(filename_W);
	} else {
		//we are using an existing vnode
		KASSERT(openFile == openedVnode);

		if (fd >= 3) {
			//inc ref count
			vnode_incref(openFile);
		}
	}

	if (table[fd] == NULL) {
		table[fd] = kmalloc(sizeof(struct file_desc));
		table[fd]->f_lock = lock_create(filename);
	}
	table[fd]->f_vn = openFile;
	table[fd]->f_name = (char *) filename;
	table[fd]->f_offset = 0;
	table[fd]->f_flags = flags;
	KASSERT(table[fd]->f_vn->vn_opencount >= 1);
	KASSERT(table[fd]->f_vn->vn_refcount >= 1);

	if (fd >= 3) {

		DEBUG(DB_A2,
				"%s created on fd = %d and vnode = %p and has vn_refcount = %d\n",
				table[fd]->f_name, fd, table[fd]->f_vn,
				table[fd]->f_vn->vn_refcount);
	}

	*retval = (int32_t) fd;
	KASSERT(curthread->t_curspl == 0);

	return 0;
}

int sys_close(int fd) {
	int dbflags = DB_A2;

	//make sure the file is in table
	KASSERT(curthread->t_curspl == 0);

	if (fd < 0 || fd >= MAX_OPEN_COUNT) {
		return EBADF;
	}

	struct file_desc* file = curthread->t_proc->fd_table[fd];

	if (curthread->t_proc->fd_table[fd] == NULL) {
		return EBADF;
	}

	KASSERT(file->f_vn->vn_opencount >= 1);
	KASSERT(file->f_vn->vn_refcount >= 1);

	if (file->f_vn->vn_refcount == 1) {

		//destroy the vnode
		if (VOP_CLOSE(file->f_vn)) {
			return EIO;
		}

	} else {
		//decr the ref count
		vnode_decref(file->f_vn);
	}
	DEBUG(DB_A2, "closed file %s\n", file->f_name);

	//clean up row in the table
	file->f_vn = NULL;
	lock_destroy(file->f_lock);
	curthread->t_proc->fd_table[fd] = NULL;
	KASSERT(file != NULL);
	kfree(file);
	KASSERT(curthread->t_proc->fd_table[fd]==NULL);

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
