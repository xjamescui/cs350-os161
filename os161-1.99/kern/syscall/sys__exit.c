#include <types.h>
#include <current.h>
#include <kern/wait.h>
#include <lib.h>
#include <spl.h>
#include <proc.h>
#include <synch.h>
#include <sys_functions.h>

#define KERNEL_PID __PID_MIN

void sys__exit(int exitcode) {
	struct proc *cur_process = curthread->t_proc;
	pid_t parent_pid = cur_process->p_parentpid;
//	int dbflags = DB_A2;
	int old_spl = splhigh();

	if (parent_pid >= KERNEL_PID && procArray[parent_pid] != NULL) { //if process has a parent

//		DEBUG(DB_A2, " I am a child with id %d and my parent is %d\n",
//				cur_process->p_pid, parent_pid);
		struct proc *parent_process = procArray[parent_pid];

		//did parent exit already? (i.e. before child)
		if (!parent_process->p_hasExited) {

			//current process is adopted by the kernel process
			//cur_process->p_parentpid = KERNEL_PID;
			//set encoded exitcode onto current process
			cur_process->p_exitcode = _MKWAIT_EXIT(exitcode);

			//tell the "interested" party about my exit
			cur_process->p_exitcode = exitcode;
			V(cur_process->p_sem_waitforcode);

			//then let waitpid capture the exitcode....
//		DEBUG(DB_A2, "stuck waiting...%d\n", cur_process->p_pid);
//		P(parent_process->p_sem_gotcode);
//		DEBUG(DB_A2, "not stuck waiting...%d\n", cur_process->p_pid);
		}

	} else {
//		DEBUG(DB_A2, "I am a child with id %d and no parent\n",
//				cur_process->p_pid);
		/**
		 * proceed to destroy process
		 */

		//detach all threads used by this process
		int processThreadCount = threadarray_num(&cur_process->p_threads);

		for (int i = 0; i < processThreadCount; i++) {
			threadarray_remove(&cur_process->p_threads, i);
		}
		KASSERT(threadarray_num(&cur_process->p_threads) == 0);

		//destroy process
		KASSERT(cur_process != NULL);
		procArray[cur_process->p_pid] = NULL;
		cur_process->p_hasExited = true;
		proc_destroy(cur_process);
		numProc--;

		if (numProc == 0) {
			V(RaceConditionSem);
		}
	}

	curthread->t_proc = NULL;
	splx(old_spl);
	thread_exit();

}
