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

/**
 * All non-file related syscall functions for A2 goes here
 *
 * Functions:
 *
 * sys__exit
 * sys_write
 *
 */

void sys__exit(int exitcode) {
//	int dbflags = DB_A2;

	(void) exitcode;
	struct proc *process = curthread->t_proc;

	KASSERT(process != NULL);

	int processCount = threadarray_num(&process->p_threads);

	//turn off all interrupts
	int original_spl = splhigh();

//	DEBUG(DB_A2, "exitcode is: %d\n", exitcode);
//	DEBUG(DB_A2, "size of threadarray is %d\n", processCount);

//detach all threads used by this process
	for (int i = 0; i < processCount; i++) {
		threadarray_remove(&process->p_threads, i);
	}

	KASSERT(threadarray_num(&process->p_threads) == 0);

	//destroy process
	proc_destroy(process);

	//turn spl back to original level
	splx(original_spl);

	thread_exit();
}

int line = 0;

int sys_write(int fd, const void *buf, size_t size) {
	int dbflags = DB_A2;
	int32_t wroteBytes = -1;
	struct vnode* vn = NULL;
	struct iovec iov;
	struct uio uio_W; //uio for writing
	char *device = NULL;


	//disable interrupts
	int old_spl = splhigh();

	switch (fd) {
	default:

		//open a vnode for console
		device = kstrdup("con:");
		vfs_open(device, O_WRONLY, 0, &vn);

		DEBUG(DB_A2, "writing.. %d\n", line);
		line++;

		//setup a uio to write buf to console
		uio_kinit(&iov, &uio_W, &buf, size, 0, UIO_WRITE);

		//write to console
		VOP_WRITE(vn, &uio_W);

		//turn old interruption level back on
		splx(old_spl);

		break;
	}

	(void) fd;

	kfree(device);

	return wroteBytes;
}

int sys_open(const char *filename, int flags, int mode) {

//	int dbflags = DB_A2;
//
//	if (filename == "con:") {
//		DEBUG(DB_A2, "flag is " + flags);
//	}
	(void) flags;
	(void) mode;
	(void) filename;

	return 0;

}
