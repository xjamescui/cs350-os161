/*
 * Copyright (c) 2013
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Process support.
 *
 * There is (intentionally) not much here; you will need to add stuff
 * and maybe change around what's already present.
 *
 * p_lock is intended to be held when manipulating the pointers in the
 * proc structure, not while doing any significant work with the
 * things they point to. Rearrange this (and/or change it to be a
 * regular lock) as needed.
 *
 * Unless you're implementing multithreaded user processes, the only
 * process that will have more than one thread is the kernel process.
 */

//#include "../include/proc.h"
//
//#include "../compile/ASST2/includelinks/machine/current.h"
//#include "../compile/ASST2/opt-A1.h"
//#include "../include/addrspace.h"
//#include "../include/current.h"
//#include "../include/lib.h"
//#include "../include/types.h"
//#include "../include/vnode.h"
//#include "opt-A2.h"

#include <types.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vnode.h>
#include "opt-A2.h"

#if OPT_A2
#include <kern/errno.h>
#include <mips/trapframe.h>
#include <spl.h>
#include <limits.h>
#endif
/*
 * The process for the kernel; this holds all the kernel-only threads.
 */
struct proc *kproc;

/*
 * Create a proc structure.
 */
static struct proc *
proc_create(const char *name) {
	struct proc *proc;

	proc = kmalloc(sizeof(*proc));
	if (proc == NULL) {
		return NULL;
	}
	proc->p_name = kstrdup(name);
	if (proc->p_name == NULL) {
		kfree(proc);
		return NULL;
	}

	threadarray_init(&proc->p_threads);
	spinlock_init(&proc->p_lock);

#if OPT_A2
	proc->p_parentpid = -1; //(kernel process has no parent)
	proc->hasExited = false;
	proc->p_exitcode=0;
	proc->p_waitsem = sem_create(proc->p_name, 0);
#endif

	/* VM fields */
	proc->p_addrspace = NULL;

	/* VFS fields */
	proc->p_cwd = NULL;

	return proc;
}

/*
 * Destroy a proc structure.
 *
 * Note: nothing currently calls this. Your wait/exit code will
 * probably want to do so.
 */
void proc_destroy(struct proc *proc) {
	/*
	 * You probably want to destroy and null out much of the
	 * process (particularly the address space) at exit time if
	 * your wait/exit design calls for the process structure to
	 * hang around beyond process exit. Some wait/exit designs
	 * do, some don't.
	 */

	KASSERT(proc != NULL);
	KASSERT(proc != kproc);

	/*
	 * We don't take p_lock in here because we must have the only
	 * reference to this structure. (Otherwise it would be
	 * incorrect to destroy it.)
	 */

	/* VFS fields */
	if (proc->p_cwd) {
		VOP_DECREF(proc->p_cwd);
		proc->p_cwd = NULL;
	}

	/* VM fields */
	if (proc->p_addrspace) {
		/*
		 * In case p is the currently running process (which
		 * it might be in some circumstances, or if this code
		 * gets moved into exit as suggested above), clear
		 * p_addrspace before calling as_destroy. Otherwise if
		 * as_destroy sleeps (which is quite possible) when we
		 * come back we'll be calling as_activate on a
		 * half-destroyed address space. This tends to be
		 * messily fatal.
		 */
		struct addrspace *as;

		as_deactivate();
		as = curproc_setas(NULL);
		as_destroy(as);
	}

	threadarray_cleanup(&proc->p_threads);
	spinlock_cleanup(&proc->p_lock);

#if OPT_A2
	sem_destroy(proc->p_waitsem);
#endif

	kfree(proc->p_name);
	kfree(proc);

}

/*
 * Create the process structure for the kernel.
 */
void proc_bootstrap(void) {
#if OPT_A2
// Initialize process table
//EDIT: dynamically change pid table size
	procArraySize = 300;
	numProc = 1;
	procArray = kmalloc(sizeof(struct proc*) * procArraySize);
	for (int i = __PID_MIN; i < procArraySize; i++) {
		procArray[i] = NULL;
	}
	forkLock = lock_create("forkLock");
#endif

	kproc = proc_create("[kernel]");
	if (kproc == NULL) {
		panic("proc_create for kproc failed\n");
	}
#if OPT_A1
	kprintf("kproc = %p\n", kproc);
#endif

}

/*
 * Create a fresh proc for use by runprogram.
 *
 * It will have no address space and will inherit the current
 * process's (that is, the kernel menu's) current directory.
 */
struct proc *
proc_create_runprogram(const char *name) {
	struct proc *proc;

	proc = proc_create(name);
	if (proc == NULL) {
		return NULL;
	}

	/* VM fields */

	proc->p_addrspace = NULL;

	/* VFS fields */

	spinlock_acquire(&curproc->p_lock);
	/* we don't need to lock proc->p_lock as we have the only reference */
	if (curproc->p_cwd != NULL) {
		VOP_INCREF(curproc->p_cwd);
		proc->p_cwd = curproc->p_cwd;
	}
	spinlock_release(&curproc->p_lock);

	return proc;
}

/*
 * Add a thread to a process. Either the thread or the process might
 * or might not be current.
 */
int proc_addthread(struct proc *proc, struct thread *t) {
	int result;

	KASSERT(t->t_proc == NULL);

	spinlock_acquire(&proc->p_lock);
	result = threadarray_add(&proc->p_threads, t, NULL);
	spinlock_release(&proc->p_lock);
	if (result) {
		return result;
	}
	t->t_proc = proc;
	return 0;
}

/*
 * Remove a thread from its process. Either the thread or the process
 * might or might not be current.
 */
void proc_remthread(struct thread *t) {
	struct proc *proc;
	unsigned i, num;

	proc = t->t_proc;
	KASSERT(proc != NULL);
	spinlock_acquire(&proc->p_lock);
	/* ugh: find the thread in the array */
	num = threadarray_num(&proc->p_threads);
	for (i = 0; i < num; i++) {
		if (threadarray_get(&proc->p_threads, i) == t) {
			threadarray_remove(&proc->p_threads, i);
			spinlock_release(&proc->p_lock);
			t->t_proc = NULL;
			return;
		}
	}
	/* Did not find it. */
	spinlock_release(&proc->p_lock);
	panic("Thread (%p) has escaped from its process (%p)\n", t, proc);
}

/*
 * Fetch the address space of the current process. Caution: it isn't
 * refcounted. If you implement multithreaded processes, make sure to
 * set up a refcount scheme or some other method to make this safe.
 */
struct addrspace *
curproc_getas(void) {
	struct addrspace *as;
#ifdef UW
	/* Until user processes are created, threads used in testing
	 * (i.e., kernel threads) have no process or address space.
	 */
	if (curproc == NULL) {
		return NULL;
	}
#endif

	spinlock_acquire(&curproc->p_lock);
	as = curproc->p_addrspace;
	spinlock_release(&curproc->p_lock);
	return as;
}

/*
 * Change the address space of the current process, and return the old
 * one.
 */
struct addrspace *
curproc_setas(struct addrspace *newas) {
	struct addrspace *oldas;
	struct proc *proc = curproc;

	spinlock_acquire(&proc->p_lock);
	oldas = proc->p_addrspace;
	proc->p_addrspace = newas;
	spinlock_release(&proc->p_lock);
	return oldas;
}

#if OPT_A2
pid_t childProc_create(const char *name, struct trapframe *tf) {
	int oldspl = splhigh();
	struct proc *childProc;

	//initialize a child process
	childProc = kmalloc(sizeof(struct proc));
	if (childProc == NULL) {
		return ENOMEM;
	}

	//set the name for the child
	childProc->p_name = kstrdup(name);
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
	childProc->p_waitsem = sem_create(childProc->p_name, 0);

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
	splx(oldspl);
	KASSERT(curthread->t_curspl == 0);
	//Fork child thread (tf, addrspace)

	thread_fork("childThread", childProc, childPrep, (void *) childtf,
			(unsigned long) childas);

	return (pid_t) pidinit;
}
#endif
