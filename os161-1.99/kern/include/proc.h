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

#ifndef _PROC_H_
#define _PROC_H_

/*
 * Definition of a process.
 *
 * Note: curproc is defined by <current.h>.
 */

#include <spinlock.h>
#include <thread.h> /* required for struct threadarray */
#include <file_desc.h>
#include <kern/limits.h>
#include "opt-A2.h"
#include "opt-A3.h"

#ifdef OPT_A2
#include <mips/trapframe.h>
#include <synch.h>
//struct lock *forkLock;
#endif

struct addrspace;
struct vnode;
struct file_desc;

#define MAX_OPEN_COUNT __OPEN_MAX

struct semaphore *RaceConditionSem; //to be used in menu.c
/*
 * Process structure.
 */

#if OPT_A3

/* keep track of the ELF file loaded by load_elf */
struct elf{
	char * elf_name;
	uint32_t elf_text_offset;
	size_t elf_text_memsz;
	uint32_t elf_data_offset;
	size_t elf_data_memsz;
};

#endif

struct proc {

#if OPT_A2
	pid_t p_pid; /* process id */
	pid_t p_parentpid; /* parent process id*/
	bool p_hasExited; /* did this proc exit */
	int p_exitcode; /* exit code */
	struct file_desc *fd_table[MAX_OPEN_COUNT]; /* File Descriptor Table for this process */
	struct semaphore* p_sem_waitforcode; /* used when waiting for exit */
#endif

	char *p_name; /* Name of this process */
	struct spinlock p_lock; /* Lock for this structure */
	struct threadarray p_threads; /* Threads in this process */

	/* VM */
	struct addrspace *p_addrspace; /* virtual address space */
	struct pt* p_page_table;

	/* VFS */
	struct vnode *p_cwd; /* current working directory */

	/* ELF */
	struct elf* p_elf;
/* add more material here as needed */
};

#if OPT_A2
/**
 * Process Table
 */
struct proc **procArray;
int procArraySize;
volatile int numProc;
#endif

/* This is the process structure for the kernel and for kernel-only threads. */
extern struct proc *kproc;

/* Call once during system startup to allocate data structures. */
void proc_bootstrap(void);

/* Create a fresh process for use by runprogram(). */
struct proc *proc_create_runprogram(const char *name);

/* Destroy a process. */
void proc_destroy(struct proc *proc);

/* Attach a thread to a process. Must not already have a process. */
int proc_addthread(struct proc *proc, struct thread *t);

/* Detach a thread from its process. */
void proc_remthread(struct thread *t);

/* Fetch the address space of the current process. */
struct addrspace *curproc_getas(void);

/* Change the address space of the current process, and return the old one. */
struct addrspace *curproc_setas(struct addrspace *);

#endif /* _PROC_H_ */
