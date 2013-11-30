#include <types.h>
#include <current.h>
#include <proc.h>
#include <lib.h>
#include <copyinout.h>
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
#include <sys_functions.h>

#define MODE_STDIN 1336
#define MODE_STDOUT 1337
#define MODE_STDERR 1338
/**
 * All file related syscall functions for A2 goes here
 *
 * Functions:
 *
 * sys_read
 * sys_write
 * sys_open
 * sys_close
 *
 */

/**
 * Return value: nbytes read is returned.
 * This count should be positive.
 * A return value of 0 should be construed as signifying end-of-file.
 */
int sys_read(int fd, void* buf, size_t nbytes, int32_t *retval) {

	int readBytes = 0;
	struct file_desc* file;
	struct iovec iov;
	struct uio uio_R; //uio for reading
	int errno = -1;
	void* kernbuf = kmalloc(nbytes * sizeof(*buf)); //kernbuf to copy buf into, and also serves as a testimony for buf's validity

	/**
	 * Check that buf is a valid ptr
	 *
	 * Make use of the copycheck function available inside copyin
	 * just copy buf into kernel space and work with the kernel version of it ( we know if
	 * copyin is successful then buf is valid)
	 *
	 * Don't forget to copyout when done (to update value at the original userspace location)
	 */
	if (copyin(buf, kernbuf, nbytes)) {
		return EFAULT;
	}

	//check for valid fd
	if (fd < 0 || fd > MAX_OPEN_COUNT - 1) {
		return EBADF;
	}

	//reading from stdin
	if (fd == STDIN_FILENO) {
		if ((errno = sys_open("con:", O_RDONLY, MODE_STDIN, &fd))) {
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

	//setup uio to read
	uio_kinit(&iov, &uio_R, kernbuf, nbytes, file->f_offset, UIO_READ);

	//Reading...
	if ((errno = VOP_READ(file->f_vn, &uio_R))) {
		return errno;
	}
	readBytes = nbytes - uio_R.uio_resid;
	file->f_offset = uio_R.uio_offset;

	*retval = readBytes;
	KASSERT(curthread->t_curspl == 0);

	//we do not forget to copyout and update the content at the correct userspace addr
	copyout((const void *) kernbuf, (userptr_t) buf, nbytes);
	kfree(kernbuf);

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

	int wroteBytes = 0;
	int errno = -1;
	struct file_desc* file = NULL; //file
	struct iovec iov;
	struct uio uio_W; //uio for writing
	void* kernbuf = kmalloc(nbytes * sizeof(*buf)); //kernbuf to copy buf into, and also serves as a testimony for buf's validity

	//check for valid fd
	if (fd < 0 || fd > MAX_OPEN_COUNT - 1) {
		return EBADF;
	}

	/**
	 * Check that buf is a valid ptr
	 *
	 * Make use of the copycheck function available inside copyin
	 * just copy buf into kernel space and work with the kernel version of it ( we know if
	 * copyin is successful then buf is valid)
	 *
	 * Don't forget to copyout when done (to update value at the original userspace location)
	 */
	if (copyin(buf, kernbuf, nbytes)) {
		return EFAULT;
	}

	switch (fd) {
	case STDOUT_FILENO:
		if ((errno = sys_open("con:", O_WRONLY, MODE_STDOUT, &fd))) {
			return errno;
		}
		break;
	case STDERR_FILENO:
		if ((errno = sys_open("con:", O_WRONLY, MODE_STDERR, &fd))) {
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

	//setup uio to write to file
	uio_kinit(&iov, &uio_W, (void *) kernbuf, nbytes, file->f_offset,
			UIO_WRITE);

	//writing....
	if ((errno = VOP_WRITE(file->f_vn, &uio_W))) {
		return errno;
	}

	wroteBytes = nbytes - uio_W.uio_resid;
	file->f_offset = uio_W.uio_offset;

	*retval = wroteBytes;
	KASSERT(curthread->t_curspl == 0);

	//we do not forget to copyout and update the content at the correct userspace addr
	copyout((const void *) kernbuf, (userptr_t) buf, nbytes);
	kfree(kernbuf);

	return 0;
}

int sys_open(const char *filename, int flags, int mode, int32_t *retval) {

	int fd = -1; //filehandle to be returned
	struct file_desc** table = curthread->t_proc->fd_table;
	struct vnode* openedVnode = NULL;

	//check if filename is a valid pointer
	if (filename != "con:") {

		/**
		 * Pretty much see if we can copy this string from current userspace into kernel space;
		 * the copycheck function inside copyinstr will return an err if the pointer is invalid.
		 */
		char kfilename[MAX_LEN_FILENAME];
		size_t actual;
		if (copyinstr((const_userptr_t) filename, kfilename,
		MAX_LEN_FILENAME, &actual)) {
			return EFAULT;
		}

	}

	//check if flags is valid
	if (flags < 0 || flags > 64) {
		return EINVAL;
	}

	//if file is the console: stdin, stdout, stderr
	if (filename == "con:") {
		if (flags == O_RDONLY) {
			fd = STDIN_FILENO;	//0
		} else if (flags == O_WRONLY && mode == MODE_STDOUT) {
			fd = STDOUT_FILENO; //1
		} else if (flags == O_WRONLY && mode == MODE_STDERR) {
			fd = STDERR_FILENO; //2
		}
	}

	KASSERT(MAX_OPEN_COUNT > 4);

	//check in fd_table to see if file is already open
	if (fd < 3 && fd >= 0) {

		//standard console related (0,1,2)
		KASSERT(filename == "con:");
		if (table[fd] != NULL) {
			openedVnode = table[fd]->f_vn;
		}

	} else { //file related (3 - (n-1))

		//check if we reached MAX_OPEN_COUNT
		fd = 3;
		while (fd < MAX_OPEN_COUNT && table[fd] != NULL) {
			fd++;
		}
		//check to see if the row we found is valid
		if (fd >= MAX_OPEN_COUNT) {
			return EMFILE;
		}

		for (int i = 3; i < MAX_OPEN_COUNT; i++) {

			if (table[i] == NULL) {
				break;
			}
			if (table[i]->f_name == filename) {
				openedVnode = table[i]->f_vn;
			}
		}

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

	//open a new file
	if (table[fd] == NULL) {
		table[fd] = kmalloc(sizeof(struct file_desc));
	}
	table[fd]->f_vn = openFile;
	table[fd]->f_name = (char *) filename;
	table[fd]->f_offset = 0;
	table[fd]->f_flags = flags;
	KASSERT(table[fd]->f_vn->vn_opencount >= 1);
	KASSERT(table[fd]->f_vn->vn_refcount >= 1);

	*retval = (int32_t) fd;
	KASSERT(curthread->t_curspl == 0);

	return 0;
}

/**
 * sys_close
 */
int sys_close(int fd) {

	//make sure the file is in table
//	KASSERT(curthread->t_curspl == 0);

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

	//clean up row in the table
	file->f_vn = NULL;
	curthread->t_proc->fd_table[fd] = NULL;
	KASSERT(file != NULL);
	kfree(file);
	KASSERT(curthread->t_proc->fd_table[fd]==NULL);

	return 0;
}
