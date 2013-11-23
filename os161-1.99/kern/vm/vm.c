//#ifdef UW
/* This was added just to see if we can get things to compile properly and
 * to provide a bit of guidance for assignment 3 */

#include "opt-vm.h"
#if OPT_VM

#include <types.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include "opt-A3.h"

#if OPT_A3
#include <spl.h>
#include <current.h>
#include <proc.h>
#include <kern/errno.h>
#include <mips/tlb.h>
#include <spinlock.h>
#include <uw-vmstats.h>

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

static unsigned int next_victim = 0;

#endif

#if OPT_A3

struct lock * coremapLock;

struct page * coremap;

int NUM_PAGES= -1;

vmInitialized = 0;

paddr_t alloc_page(void) {
	//FIFO algo
	if(vmInitialized == 1) {
		lock_acquire(coremapLock);
		
		//add stuff here
		for(int a = 0 ; a < NUM_PAGES ; a++) {
			if(coremap[a].state == 0) {
				//do something	
			}	
		}	
		lock_release(coremapLock);
	}
}

paddr_t alloc_pages(int npages) {
	if(vmInitialized == 1) {
		lock_acquire(coremapLock);
		
		//add stuff here	
		lock_release(coremapLock);
	}
}

void free_page(vaddr_t addr) {
	if(vmInitialized == 1) {
		lock_acquire(coremapLock);
		
		//add stuff here	
		lock_release(coremapLock);
	}
}

#endif

void vm_bootstrap(void) {
	/* May need to add code. */
	#if OPT_A3	
	paddr_t* first;
	paddr_t* last;
	coremapLock = lock_create("coremapLock");
	//TODO maybe add check to see if we got a lock
	if(coremapLock == NULL) {
		panic("Cannot get coremapLock!");
	}	

	ram_getsize(first,last);

	//we are setting the start of coremap tot eh first available mem spot specified in first
	coremap = (struct page *) PADDR_TO_KVADDR(first);
	
	//paddr_t origFirst = *first;	

	//now the first free mem page is shifted from page by the size of the coremap
	first = first + (struct page *) numPages * sizeof(struct page);

	//4K page size
	NUM_PAGES = (first-last)/4096;

	//initialize a dummy page	
	struct page * pg;
	pg.as = NULL;
	pg.paddr = NULL;
	pg.vaddr = NULL;
	pg.vpn = -1;
	pg.state = 0; //state should still be free

	//init coremap
	for (int a = 0 ; a < NUM_PAGES ; a++) {
		coremap[a] = pg;
	}

	//we initialized the vm
	vmInitialized = 1;

	#endif
}

//#if 0
/* You will need to call this at some point */
paddr_t getppages(unsigned long npages) {
#if OPT_A3
	paddr_t addr;

	spinlock_acquire(&stealmem_lock);
	
	//TODO instead of this, we call our mem allocator
	addr = ram_stealmem(npages);

	spinlock_release(&stealmem_lock);
	return addr;
#else
	/* Adapt code form dumbvm or implement something new */
	(void) npages;
	panic("Not implemented yet.\n");
	return (paddr_t) NULL;
#endif
}
//#endif

/* Allocate/free some kernel-space virtual pages */
vaddr_t alloc_kpages(int npages) {
#if OPT_A3
	paddr_t pa;
	
	pa = getppages(npages);
	
	if (pa == 0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
#else
	/* Adapt code form dumbvm or implement something new */
	(void) npages;
	panic("Not implemented yet.\n");
	return (vaddr_t) NULL;
#endif
}

void free_kpages(vaddr_t addr) {
	/* nothing - leak the memory. */

	(void) addr;
}

void vm_tlbshootdown_all(void) {
#if OPT_A3
	panic("dumbvm tried to do tlb shootdown?!\n");
#else
	panic("Not implemented yet.\n");
#endif
}

void vm_tlbshootdown(const struct tlbshootdown *ts) {
	(void) ts;
#if OPT_A3
	panic("dumbvm tried to do tlb shootdown?!\n");
#else
	panic("Not implemented yet.\n");
#endif
}

/**
 * Given algorithm as replacement policy in TLB
 */
int tlb_get_rr_victim() {
	int victim = next_victim;
	next_victim = (next_victim + 1) % NUM_TLB;
	return victim;
}

/**
 * vm_fault should avoid anything that involves touching virtual addresses
 * in the application's part of the virtual address space,
 * since those attempts might generate faults, depending on what's in the TLB.
 * Functions like copyin and copyout are examples of functions that touch
 * application virtual addresses.
 */
int vm_fault(int faulttype, vaddr_t faultaddress) {

#if OPT_A3
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
//	int dbflags = DB_A3;
	int i;
	int victim_index;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
		case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */
		panic("dumbvm: got VM_FAULT_READONLY\n");
		case VM_FAULT_READ:
//		DEBUG(DB_A3, "this is a VM_FAULT_READ\n");
//		break;
		case VM_FAULT_WRITE:
//		DEBUG(DB_A3, "this is a VM_FAULT_WRITE\n");
		break;
		default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = curproc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = (faultaddress - vbase1) + as->as_pbase1;
	} else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_pbase2;
	} else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
	} else {
		return EFAULT;
	}

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i = 0; i < NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

	//all tlb entries are valid, time to do "replacement" i.e. to make room
	victim_index = tlb_get_rr_victim();
	tlb_write(TLBHI_INVALID(victim_index), TLBLO_INVALID(), victim_index);

	//write to the victim's entry we just invalidated
	ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	tlb_write(ehi, elo, i);

	//stats update: TLB replace
	vmstats_inc(VMSTAT_TLB_FAULT_REPLACE);

	splx(spl);
	return 0;

	kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;
#else
	/* Adapt code form dumbvm or implement something new */
	(void) faulttype;
	(void) faultaddress;
	panic("Not implemented yet.\n");
	return 0;
#endif
}
#endif /* OPT_VM */

//#endif /* UW */

