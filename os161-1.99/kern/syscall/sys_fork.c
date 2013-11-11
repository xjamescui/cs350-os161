#include <types.h>
#include <current.h>
#include <proc.h>
#include <spl.h>
#include <sys_functions.h>
#include <kern/errno.h>
#include <limits.h>
#include <addrspace.h>
#include <vnode.h>

pid_t sys_getpid(void) {
	return curthread->t_proc->p_pid;
}

pid_t sys_fork(struct trapframe *tf, int32_t *retval) {

	int oldspl = splhigh();
//	lock_acquire(forkLock);
	struct proc *childProc;

	//initialize a child process
	childProc = kmalloc(sizeof(struct proc));
	if (childProc == NULL) {
		return ENOMEM;
	}

	//set the name for the child
	childProc->p_name = kstrdup("childProc");
	if (childProc->p_name == NULL) {
		kfree(childProc);
		return ENOMEM;
	}

	//Copy trapframe onto kernel heap
	struct trapframe *childtf = kmalloc(sizeof(struct trapframe));
	if (childtf == NULL) {
		kfree(childProc->p_name);
		kfree(childProc);
		return ENOMEM;
	}
	//copy_trapframe(tf, childtf);
	memcpy(childtf, tf, sizeof(struct trapframe));

	//Initialize threadarray?
	threadarray_init(&childProc->p_threads);
	spinlock_init(&childProc->p_lock);
	childProc->p_parentpid = curproc->p_pid;
	childProc->p_exitcode = 0;
	childProc->p_sem_waitforcode = sem_create(childProc->p_name, 0);

	//Copy addrspace
	struct addrspace *childas;
	if (0 != as_copy(curproc_getas(), &childas)) {
		kfree(childProc->p_name);
		kfree(childProc);
		kfree(childtf);
		return ENOMEM;
	};

	//Copy CWD
	childProc->p_cwd = curproc->p_cwd;
	childProc->p_cwd->vn_refcount++;

	//Copy over fd table
	for (int i = 0; i < MAX_OPEN_COUNT; i++) {
		childProc->fd_table[i] = curproc->fd_table[i];
	}

	for (int i = 3; i < MAX_OPEN_COUNT; i++) {

		//double file ref count (now both child and parent share the fd_table)
		if (childProc->fd_table[i] != NULL) {
			childProc->fd_table[i]->f_vn->vn_refcount *= 2;
		}
	}

	//Get PID
	int pidinit = 0;
	for (int i = __PID_MIN; i < procArraySize; i++) {
		if (procArray[i] == NULL) {
			childProc->p_pid = i;
			procArray[i] = childProc;
			pidinit = i;
			numProc++;
			break;
		}
	}
	if (pidinit == 0) {
		//No free space in process table
		//EDIT: Need better handling. No panicing.
		return ENPROC;
	}
	*retval = pidinit;

	//lock_release(forkLock);
	splx(oldspl);
	KASSERT(curthread->t_curspl == 0);
	//Fork child thread (tf, addrspace)

	thread_fork("childThread", childProc, childPrep, (void *) childtf,
			(unsigned long) childas);

	return 0;
}
