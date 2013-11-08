#include <types.h>
#include <current.h>
#include <kern/wait.h>
#include <kern/errno.h>
#include <sys_functions.h>

pid_t sys_waitpid(pid_t pid, int *status, int options) {

	//check if pid argument named a process that the current process
	//was not interested in or that has not yet exited.
//	if(pid == curthread->t_proc->p_id){
//		return ECHILD;
//	}
	//check if pid argument named a nonexistent process.
	if (false) {
		return ESRCH;
	}

	//check status
	if (status == NULL || ((uint32_t) status % 4 != 0)) {
		return EFAULT;
	}

	//check options
	if (options != 0) {
		return EINVAL;
	}


	//wait while the "interested" party is still running
	/**
	 * declare a cv in proc struct
	 * cv_wait here if interested party is still running
	 * In sys__exit: cv_signal just before thread_exit()
	 */


	//returns the process id whose exit status is reported in status. In OS/161, this is always the value of pid.
	return pid;
}
