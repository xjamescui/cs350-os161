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

__off_t offset = 0;

int sys_write(int fd, const void *buf, size_t size) {
//	int dbflags = DB_A2;
	int32_t wroteBytes = 0;
	struct vnode* vn = NULL;
	struct iovec iov;
	struct uio uio_W; //uio for writing
	char *device = NULL;

	//disable interrupts
	int old_spl = splhigh();
	wroteBytes = kprintf(buf);
	switch (fd) {
	case STDOUT_FILENO: //stdout
		wroteBytes = kprintf(buf); //temporary use kprintf
		break;
	default:
		vfs_open(device, O_WRONLY, 0664, &vn);

		//		DEBUG(DB_A2, "writing.. %d\n", (int)offset);

		//setup a uio to write buf to console
		uio_kinit(&iov, &uio_W, &buf, size, 0, UIO_WRITE);

		//write to console
		wroteBytes = VOP_WRITE(vn, &uio_W);
		//		DEBUG(DB_A2, "fd is %d, writing.. %d\n", fd, (int )uio_W.uio_resid);

		//		offset = uio_W.uio_offset;
		//turn old interruption level back on
		splx(old_spl);
		break;
	}
	(void) buf;
	(void) size;
	(void) fd;

	splx(old_spl);

	return wroteBytes;
}

int sys_open(const char *filename, int flags, int mode) {

	struct vnode* file;
	int fd = 0; //filehandle to be returned
	bool opened = false;
	struct file_desc** table = curthread->fd_table;

	//if file is the console: stdin, stdout, stderr
	if (filename == "con:" && flags == O_RDONLY) {
		fd = STDIN_FILENO;	//0
	} else if (filename == "con:" && flags == O_WRONLY && mode == 0664) {
		fd = STDOUT_FILENO; //1
	} else if (filename == "con:" && flags == O_WRONLY && mode == 0665) {
		fd = STDERR_FILENO; //2
	} else {

		//check in fd_table to see if file is already open
		KASSERT(MAX_OPEN_COUNT > 4);
		for (int i = 3; i < MAX_OPEN_COUNT; i++) {

			if (table[i] == NULL) {
				break;
			}

			/**
			 * todo: f_name should have a max of MAX_LEN_FILENAME letters
			 */
			if (table[i]->f_name == filename) {
				opened = true;
				table[i]->f_opencount++;
				fd = i;
				//should we also incrememnt reference count?
			}

		}

		//if not already opened
		if (!opened) {

			//check if we reached MAX_OPEN_COUNT
			fd = 3;
			while (fd < MAX_OPEN_COUNT && table[fd] != NULL) {
				fd++;
			}

			if (fd < MAX_OPEN_COUNT) {
				//open file and add entry to fdtable
				if (vfs_open((char *) filename, flags, mode, &file)) {
					//if there is a problem opening the file
					// errno =
					return -1;
				}

				table[fd] = kmalloc(sizeof(struct file_desc));
				table[fd]->f_name = (char *) filename;
				table[fd]->f_opencount = 1;
				table[fd]->f_refcount = 1;
				table[fd]->f_vn = file;
				table[fd]->f_lock = lock_create(filename);
			}
		}
	}

	return fd;

}

#if OPT_A2
///**
// * For Debugging Only
// */
//void printTable(){
//
//}
#endif
