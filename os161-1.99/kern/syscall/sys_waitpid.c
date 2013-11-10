#include <types.h>
#include <current.h>
#include <lib.h>
#include <kern/wait.h>
#include <proc.h>
#include <kern/errno.h>
#include <sys_functions.h>

pid_t sys_waitpid(pid_t pid, int *status, int options, int32_t *retval) {

	struct proc* childProc = procArray[pid]; //the process we are waiting on
	int dbflags = DB_A2;

	//check if pid argument named a nonexistent process.
	if (childProc == NULL) {
		DEBUG(DB_A2, "A problem\n");
		return ESRCH;
	}

	//check if pid argument named a process that the current process
	//was not interested in or that has not yet exited.
	if (pid == (pid_t) curthread->t_proc->p_pid) {
		DEBUG(DB_A2, "B problem\n");
		return ECHILD;
	}

	//check status
	if (status == NULL || ((uint32_t) status % 4 != 0)) {
		DEBUG(DB_A2, "C problem\n");
		return EFAULT;
	}

	//check options
	if (options != 0) {
		DEBUG(DB_A2, "D problem\n");
		return EINVAL;
	}

	//wait while targetProc is still running
	if (childProc->p_exitcode == 0) {
		P(childProc->p_waitsem);
		KASSERT(childProc->p_waitsem->sem_count == 0);
	}

	//set the exit status
	*status = childProc->p_exitcode;

	/**
	 * cleanup targetProc from process table, because it has finished
	 *
	 */

	//detach all threads used by this process
	int processThreadCount = threadarray_num(&childProc->p_threads);

	for (int i = 0; i < processThreadCount; i++) {
		threadarray_remove(&childProc->p_threads, i);
	}
	KASSERT(threadarray_num(&childProc->p_threads) == 0);

	childProc->hasExited = true;
	proc_destroy(childProc);
	procArray[pid] = NULL;
	numProc--;

	//returns the process id whose exit status is reported in status. In OS/161, this is always the value of pid.
	*retval = pid;
	return 0;
}
