#include <types.h>
#include <current.h>
#include <lib.h>
#include <kern/wait.h>
#include <proc.h>
#include <copyinout.h>
#include <kern/errno.h>
#include <sys_functions.h>

pid_t sys_waitpid(pid_t pid, int *status, int options, int32_t *retval) {

	struct proc* runningProc = procArray[pid]; //the process we are waiting on

	//check if pid argument named a nonexistent process.
	if (pid < __PID_MIN || pid > __PID_MAX || runningProc == NULL) {
		return ESRCH;
	}

	/**
	 * 	check if pid argument named a process that the current process
	 * 	was not interested in or that has not yet exited.
	 *
	 *  cases:
	 *  - cant wait on urself
	 *  - cant wait on someone that is not your child
	 *  - cant wait on siblings
	 */

	if (pid == (pid_t) curthread->t_proc->p_pid
			|| runningProc->p_parentpid != (pid_t) curthread->t_proc->p_pid) {
		return ECHILD;
	}

	//check that we are not waiting on siblings
	if (runningProc->p_parentpid == curthread->t_proc->p_parentpid) {
		return ECHILD;
	}

	//check for proper alignment
	if ((uint32_t) status % 4 != 0) {
		return EFAULT;
	}

	//check status
	//if we can copy ptr to kernel space successfully then it is a valid pointer
	int *kstatus = kmalloc(sizeof(int*));
	if (copyin((const_userptr_t) status, kstatus, sizeof(status))) {
		return EFAULT;
	}
	kfree(kstatus);

	//check options
	if (options != 0) {
		return EINVAL;
	}

	//wait while targetProc is still running
	if (!runningProc->p_hasExited) {
		P(runningProc->p_sem_waitforcode);
		KASSERT(runningProc->p_sem_waitforcode->sem_count == 0);
	}

	//set the exit status
	*status = runningProc->p_exitcode;

	/**
	 * Clean up exited process
	 */

	//detach all threads used by the child process
	int processThreadCount = threadarray_num(&runningProc->p_threads);

	for (int i = 0; i < processThreadCount; i++) {
		threadarray_remove(&runningProc->p_threads, i);
	}
	KASSERT(threadarray_num(&runningProc->p_threads) == 0);

	//destroy child process
	KASSERT(runningProc != NULL);
	procArray[runningProc->p_pid] = NULL;
	runningProc->p_hasExited = true;
	proc_destroy(runningProc);
	numProc--;

	//returns the process id whose exit status is reported in status. In OS/161, this is always the value of pid.
	*retval = pid;
	return 0;
}
