#include <types.h>
#include <kern/wait.h>
#include <sys_functions.h>

pid_t sys_waitpid(pid_t pid, int *status, int options){


	(void )pid;
	(void) status;
	(void) options;
	return 0;
}
