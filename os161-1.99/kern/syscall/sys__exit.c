#include <types.h>
#include <current.h>
#include <kern/wait.h>
#include <proc.h>
#include <synch.h>
#include <sys_functions.h>

void sys__exit(int exitcode) {
	struct proc *cur_process = curthread->t_proc;

	//KASSERT(cur_process != NULL);

	//detach all threads used by this process
	int processThreadCount = threadarray_num(&cur_process->p_threads);

	for (int i = 0; i < processThreadCount; i++) {
		threadarray_remove(&cur_process->p_threads, i);
	}
	KASSERT(threadarray_num(&cur_process->p_threads) == 0);

	//set encoded exitcode onto current process
	cur_process->p_exitcode = _MKWAIT_EXIT(exitcode);



	//destroy process
	KASSERT(cur_process != NULL);
	proc_destroy(cur_process);
	curthread->t_proc = NULL;

	//tell the "interested" party about my exit
	if(numProc == 1) {
		V(RaceConditionSem);
	}
	numProc--;
	thread_exit();

}
