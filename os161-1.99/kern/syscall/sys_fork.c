#include <types.h>
#include <current.h>
#include <proc.h>
#include <spl.h>
#include <sys_functions.h>

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
 lock_acquire(forkLock);
 //Create new process
 *retval = childProc_create("childProc", tf);
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
 lock_release(forkLock);
 //splx(old_spl);
 KASSERT(curthread->t_curspl == 0);

	return 0;
}
