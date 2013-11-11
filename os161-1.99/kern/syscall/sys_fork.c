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

	/**
	 * Note: child should be "born" with the same state as the parent
	 *
	 * child's address space = copy of(parent's address space)
	 * return child_pid to parent
	 * return 0 to child_pid
	 * child_pid != parent_pid
	 */

	//struct proc* parent = curthread->t_proc;

	//turn off interrupts
	//int old_spl = splhigh();
 //lock_acquire(forkLock);
 //Create new process
 //*retval = childProc_create("childProc", tf);
 //EDIT: Copy file table
	/**
	 * copy parent's fd_table to child
	 * Note: However, the file handle objects the file tables point to are shared,
	 * so, for instance, calls to lseek in one process can affect the other.
	 */

	//turn interrupts back on
	//splx(old_spl);
	//KASSERT(curthread->t_curspl == 0);

	//return
	//(void) parent;
 //*retval = childProc->p_pid;
 //lock_release(forkLock);
 //splx(old_spl);
 //KASSERT(curthread->t_curspl == 0);

	//int oldspl = splhigh();
 lock_acquire(forkLock);
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
	//childtf->tf_k0 = 0;
	//childtf->tf_k1 = 0;

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

 lock_release(forkLock);
	//splx(oldspl);
	KASSERT(curthread->t_curspl == 0);
	//Fork child thread (tf, addrspace)

	thread_fork("childThread", childProc, childPrep, (void *) childtf,
			(unsigned long) childas);



	return 0;
}
