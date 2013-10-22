#include <sys_functions.h>
#include <copyinout.h>
#include <types.h>
#include <current.h>
#include <proc.h>
#include <lib.h>

/**
 * All non-file related syscall functions for A2 goes here
 *
 * Functions:
 *
 * sys___exit
 *
 *
 */

void sys__exit(int exitcode) {

	kprintf("exitcode is: %d", exitcode);
	proc_destroy(curthread->t_proc);

}

int sys_write(int filehandle, const void *buf, size_t size) {
	(void) filehandle;
	(void) size;
	int wroteBytes = kprintf(buf);
	return wroteBytes;
}

