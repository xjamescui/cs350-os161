#include <types.h>
#include <current.h>
#include <kern/wait.h>
#include <spl.h>
#include <proc.h>
#include <synch.h>
#include <sys_functions.h>

#define KERNEL_PID __PID_MIN

void sys__exit(int exitcode) {
	struct proc *cur_process = curthread->t_proc;
	pid_t parent_pid = cur_process->p_parentpid;

	int old_spl = splhigh();

	if (parent_pid > 0 && procArray[parent_pid] != NULL) { //if process has a parent

		struct proc *parent_process = procArray[parent_pid];

		//did parent exit already? (i.e. before child)
		if (parent_process->hasExited) {

			//current process is adopted by the kernel process
			cur_process->p_parentpid = KERNEL_PID;
		}

		//set encoded exitcode onto current process
		cur_process->p_exitcode = _MKWAIT_EXIT(exitcode);

		//tell the "interested" party about my exit
		KASSERT(cur_process->p_waitsem->sem_count == 0);
		cur_process->p_exitcode = exitcode;
		V(cur_process->p_waitsem);
		KASSERT(cur_process->p_waitsem->sem_count == 1);

		//then let waitpid destroy process....

	} else {

		/**
		 * proceed to destroy process
		 */

		//detach all threads used by this process
		int processThreadCount = threadarray_num(&cur_process->p_threads);

		for (int i = 0; i < processThreadCount; i++) {
			threadarray_remove(&cur_process->p_threads, i);
		}
		KASSERT(threadarray_num(&cur_process->p_threads) == 0);

		cur_process->hasExited = true;

		//destroy process
		KASSERT(cur_process != NULL);
		proc_destroy(cur_process);
		curthread->t_proc = NULL;
		numProc--;
	}

	if (numProc == 0) {
		V(RaceConditionSem);
	}

	splx(old_spl);
	thread_exit();

}
