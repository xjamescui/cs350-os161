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
//		DEBUG(DB_A2, "I am waiting for my child\n");
		P(childProc->p_sem_waitforcode);
		KASSERT(childProc->p_sem_waitforcode->sem_count == 0);
	}

	//set the exit status
	*status = childProc->p_exitcode;

	KASSERT(childProc->p_sem_gotcode->sem_count == 0);

	//allow sys_exit to cleanup child process
	V(childProc->p_sem_gotcode);
	KASSERT(childProc->p_sem_gotcode->sem_count == 1);

	//returns the process id whose exit status is reported in status. In OS/161, this is always the value of pid.
	*retval = pid;
	return 0;
}
