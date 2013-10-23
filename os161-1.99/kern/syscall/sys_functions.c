#include <types.h>
#include <current.h>
#include <proc.h>
#include <lib.h>
#include <sys_functions.h>
#include <spl.h>

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
	int dbflags = DB_A2;

	struct proc *process = curthread->t_proc;

	KASSERT(process != NULL);

	int processCount = threadarray_num(&process->p_threads);

	//turn off all interrupts
	int original_spl = splhigh();

	DEBUG(DB_A2, "exitcode is: %d\n", exitcode);
	DEBUG(DB_A2, "size of threadarray is %d\n", processCount);

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

int sys_write(int filehandle, const void *buf, size_t size) {
	(void) filehandle;
	(void) size;
	int wroteBytes = kprintf(buf);
	return wroteBytes;
}

