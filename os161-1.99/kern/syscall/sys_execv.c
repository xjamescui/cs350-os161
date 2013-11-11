#include <types.h>
#include <proc.h>
#include <spl.h>
#include <sys_functions.h>
#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <copyinout.h>
#include <limits.h>
#include "opt-A2.h"

int sys_execv(char *progname, char **args) {

	struct addrspace *as;
	
	if(progname == NULL)	{
		return EFAULT; 	
	}
	
	int len = strlen(progname);
	
	if (len > MAX_LEN_FILENAME) {
		return EFAULT;
	}

	if (len == 0) {
		return EISDIR;
	}
	
	int counter = 0;
	while (args[counter] != NULL) {
		counter++;
	}

	int argc = counter;
	int argslen[argc];
	for(int a = 0 ; a < argc ; a++) {
		
		if((unsigned int)args[a] > 0x7fffffff) {
			return EFAULT;
		}
		
		if(args[a] != NULL) {
			argslen[a] = strlen(args[a]) +1;
		}
	}

	//kprintf("Program Name is : %s\n", progname);

	char ** kargs = kmalloc(argc*sizeof(char*));
	char * kernProgName = kmalloc(len*sizeof(char));
	size_t got;
	copyinstr((const_userptr_t)progname, kernProgName, len, &got);
	//kprintf("Copied name : %s\n", kernProgName);

	for(int a = 0 ; a < argc ; a++) {
		kargs[a] = kmalloc(argslen[a]+1*sizeof(char));
		copyinstr((const_userptr_t)args[a], kargs[a], argslen[a], &got);
	}
	kargs[argc] = NULL;

	//kprintf("Argc=%d\n", argc);
	/*
	for(int a = 0 ; a < argc ; a++) {
		kprintf("%s     Args[%d] are : %s\n",kernProgName, a, kargs[a]);
		kprintf("argg len is : %d\n", argslen[a]);
	}
	kprintf("\n\n");
	*/

	as = curproc_getas();
	as_destroy(as);
	curproc_setas(NULL);
	//kprintf("CALLING RUNPROGRAM");
	return runprogram2(kernProgName, (unsigned long)argc, kargs, NULL);
}
