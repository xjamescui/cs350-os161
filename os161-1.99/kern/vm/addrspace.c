/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
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

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <spl.h>
#include <mips/tlb.h>
#include <vm.h>
#ifdef UW
#include <proc.h>
#endif
#include "opt-A3.h"

#if OPT_A3 //dumbvm implementation of as_* functions just to start A3
#include <current.h>
#include <uw-vmstats.h>
#include <pt.h>

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

struct addrspace *
as_create(void) {
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	as->as_vbase_text = 0;
	as->as_pbase_text = 0;
	as->as_npages_text = 0;
	as->as_vbase_data = 0;
	as->as_pbase_data = 0;
	as->as_npages_data = 0;
	as->as_stackpbase = 0;


	//init page table for this address space
	as->pgTable = kmalloc(sizeof(struct pt));

	return as;
}

void as_destroy(struct addrspace *as) {
	kfree(as);
}

void as_activate(void) {
	int i, spl;
	struct addrspace *as;

	as = curproc_getas();
#ifdef UW
	/* Kernel threads don't have an address spaces to activate */
#endif
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i = 0; i < NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
	//stats update: entire TLB has been invalidated
	vmstats_inc(VMSTAT_TLB_INVALIDATE);
	splx(spl);
}

void as_deactivate(void) {
	/* nothing */

	//invalidate/clean page table?
}

int as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		int readable, int writeable, int executable, uint32_t offset) {
	size_t npages;
	size_t sz_copy = sz;
	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t) PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void) readable;
	(void) writeable;
	(void) executable;

	if (as->as_vbase_text == 0) {
		as->as_vbase_text = vaddr;
		as->as_npages_text = npages;
#if OPT_A3
		curproc->p_elf->elf_text_offset = offset;
		curproc->p_elf->elf_text_memsz = sz_copy ;
#endif
		return 0;
	}

	if (as->as_vbase_data == 0) {
		as->as_vbase_data = vaddr;
		as->as_npages_data = npages;
#if OPT_A3
		curproc->p_elf->elf_data_offset = offset;
		curproc->p_elf->elf_data_memsz = sz_copy;
#endif
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return EUNIMP;
}

static
void as_zero_region(paddr_t paddr, unsigned npages) {
	bzero((void *) PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}

int as_prepare_load(struct addrspace *as) {
	KASSERT(as->as_pbase_text == 0);
	KASSERT(as->as_pbase_data == 0);
	KASSERT(as->as_stackpbase == 0);

//	as->as_pbase1 = getppages(as->as_npages1);
//	if (as->as_pbase1 == 0) {
//		return ENOMEM;
//	}
//
//	as->as_pbase2 = getppages(as->as_npages2);
//	if (as->as_pbase2 == 0) {
//		return ENOMEM;
//	}

	as->as_stackpbase = getppages(DUMBVM_STACKPAGES);
	if (as->as_stackpbase == 0) {
		return ENOMEM;
	}

	//initialize/setup page table
//	initPT(as->pgTable);


//	as_zero_region(as->as_pbase1, as->as_npages1);
//	as_zero_region(as->as_pbase2, as->as_npages2);
	as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);

	return 0;
}

int as_complete_load(struct addrspace *as) {
	(void) as;
	return 0;
}

int as_define_stack(struct addrspace *as, vaddr_t *stackptr) {
	KASSERT(as->as_stackpbase != 0);

	*stackptr = USERSTACK;
	return 0;
}

int as_copy(struct addrspace *old, struct addrspace **ret) {
	struct addrspace *new;

	new = as_create();
	if (new == NULL) {
		return ENOMEM;
	}

	new->as_vbase_text = old->as_vbase_text;
	new->as_npages_text = old->as_npages_text;
	new->as_vbase_data = old->as_vbase_data;
	new->as_npages_data = old->as_npages_data;

	/* (Mis)use as_prepare_load to allocate some physical memory. */
	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}

	KASSERT(new->as_pbase_text != 0);
	KASSERT(new->as_pbase_data != 0);
	KASSERT(new->as_stackpbase != 0);

	memmove((void *) PADDR_TO_KVADDR(new->as_pbase_text),
			(const void *) PADDR_TO_KVADDR(old->as_pbase_text),
			old->as_npages_text * PAGE_SIZE);

	memmove((void *) PADDR_TO_KVADDR(new->as_pbase_data),
			(const void *) PADDR_TO_KVADDR(old->as_pbase_data),
			old->as_npages_data * PAGE_SIZE);

	memmove((void *) PADDR_TO_KVADDR(new->as_stackpbase),
			(const void *) PADDR_TO_KVADDR(old->as_stackpbase),
			DUMBVM_STACKPAGES * PAGE_SIZE);

	*ret = new;
	return 0;
}

#else //A3 implementations
/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	/*
	 * Write this.
	 */

	(void)old;

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */

	kfree(as);
}

void
as_activate(void)
{
	struct addrspace *as;

	as = curproc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	/*
	 * Write this.
	 */
}

void
#ifdef UW
as_deactivate(void)
#else
as_dectivate(void)
#endif
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		int readable, int writeable, int executable)
{
	/*
	 * Write this.
	 */

	(void)as;
	(void)vaddr;
	(void)sz;
	(void)readable;
	(void)writeable;
	(void)executable;
	return EUNIMP;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}
#endif
