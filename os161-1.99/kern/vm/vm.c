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
#include <coremap.h>
#include <mips/vm.h>
#include <current.h>
#include <proc.h>
#include <kern/errno.h>
#include <mips/tlb.h>
#include <spinlock.h>
#include <uw-vmstats.h>
#include <pt.h>

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

static unsigned int next_victim = 0;

#endif

void vm_bootstrap(void) {
	/* May need to add code. */
#if OPT_A3
	vmInitialized = false;
	paddr_t p_first; //lowest physical address in RAM (bottom)
	paddr_t p_last; //highest physical address in RAM (top)
	//test
	//region allocated for coremap
	paddr_t cm_low;
	paddr_t cm_high; //also the beginning of free memory i RAM

	int dbflags = DB_A3;
	NUM_PAGES = -1;
	//TODO maybe add check to see if we got a lock

	ram_getsize(&p_first, &p_last);

	KASSERT(p_last > p_first);

	//record total number of pages in physical memory
	NUM_PAGES = (p_last) / PAGE_SIZE;

	/**
	 * Allocate space needed for our coremap
	 * i.e.
	 * first to firstFree = coremap
	 * firstFree to last = free memory in RAM
	 */

	cm_low = p_first;
	DEBUG(DB_A3, "cm_low is %x\n", cm_low);
	coremap = (struct page *) PADDR_TO_KVADDR(cm_low);
	cm_high = p_first + NUM_PAGES * sizeof(struct page);
	DEBUG(DB_A3, "cm_high is %x\n", cm_high);

	/**
	 * Initialize contents in coremap
	 */

	KASSERT(NUM_PAGES > 0);
	for (int a = 0; a < NUM_PAGES; a++) {

		coremap[a].as = NULL;
		coremap[a].paddr = (paddr_t) a * PAGE_SIZE;
		coremap[a].vpn = -1;
		coremap[a].blocksAllocated = -1;
		coremap[a].id = 0;
		//mark pages between cm_high to top as FREE, else FIXED
		if (((unsigned int) a > (cm_high / PAGE_SIZE))) {
			coremap[a].state = FREE;
		} else {
			coremap[a].state = FIXED;
		}
	}

//	for (int a = 0; a < NUM_PAGES; a++) {
//
//		DEBUG(DB_A3,
//				"a = %d, PAGE_SIZE= %d, coremap[%d].state=%d, address =%x, paddr = %x, blocksAlloc = %d\n",
//				a, PAGE_SIZE, a, coremap[a].state, coremap[a].vaddr,
//				coremap[a].paddr, coremap[a].blocksAllocated);
//	}

	//we initialized the vm
	vmInitialized = true;
	coremapLock = lock_create("coremapLock");

	if (coremapLock == NULL) {
		panic("Cannot get coremapLock!");
	}

#endif
}

//#if 0
/* You will need to call this at some point */

/*

 py code for testing

 def fn (n):
 lst = [1,0,1,0,0,1,1,0,0,0,1,0,1,0,0,0,0]
 count = 0
 for i in range(len(lst)):
 if lst[i] == 0:
 count+=1
 else:
 count=0
 if count == n:
 for j in range(i-count+1,i+1):
 lst[j] = 2
 print(i-count+1)
 print(lst)
 return
 print("cannot find block large enough!")


 */

paddr_t getppages(unsigned long npages) {

#if OPT_A3
	if (vmInitialized) {
		return cm_alloc_pages(npages); //use the coremap interface to handle physical pages
	} else {
		paddr_t addr;

		spinlock_acquire(&stealmem_lock);

		addr = ram_stealmem(npages);

		spinlock_release(&stealmem_lock);
		return addr;
	}
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

	/**
	 * TODO: remember to invalidate in coremap AND owner thread's page table
	 */
	for (int a = 0; a < NUM_PAGES; a++) {
		//bitwise and addr with 0xfffff000 to align by 4kB
		if (coremap[a].vaddr == (addr & 0xfffff000) && coremap[a].state != 1) {
			for (int b = a; b < a + coremap[a].blocksAllocated; b++) {

				//need to make sure state is not fixed
				if (coremap[b].state != FIXED) {
					coremap[b].paddr = (paddr_t) NULL;
					coremap[b].state = FREE;
					//addr needed to be aligned by 4k
				}
			}
		}
	}
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
 * Prints the TLB to stdout
 */
void printTLB() {
	uint32_t ehi, elo;
	kprintf("---------Printing the TLB---------\n");
	for(int i=0; i < NUM_TLB; i++){
		tlb_read(&ehi, &elo, i);
		kprintf("TLB[%d], ehi=%x, elo=%x\n", i, ehi, elo);
	}
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
	vaddr_t vbase_text, vtop_text, vbase_data, vtop_data, stackbase, stacktop;
	paddr_t paddr;
//	int dbflags = DB_A3;
	int i;
	int victim_index;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;
	int inText = 0; //whether or not the faultaddress is in text segment




	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	case VM_FAULT_READONLY: //2
		/* We always create pages read-write, so we can't get this */
		panic("dumbvm: got VM_FAULT_READONLY\n");
	case VM_FAULT_READ: //0
		break;
	case VM_FAULT_WRITE: //1
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
	KASSERT(as->as_vbase_text != 0);
//	KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_npages_text != 0);
	KASSERT(as->as_vbase_data != 0);
//	KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages_data != 0);
//	KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase_text & PAGE_FRAME) == as->as_vbase_text);
//	KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase_data & PAGE_FRAME) == as->as_vbase_data);
//	KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

	vbase_text = as->as_vbase_text;
	vtop_text = vbase_text + as->as_npages_text * PAGE_SIZE;
	vbase_data = as->as_vbase_data;
	vtop_data = vbase_data + as->as_npages_data * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	KASSERT(as->pgTable != NULL);
//	DEBUG(DB_A3, "vm_fault: fault addr = %x, faulttype = %d\n", faultaddress,
//			faulttype);

	struct pte * entry;

	if (faultaddress >= vbase_text && faultaddress < vtop_text) {
		entry = getPTE(as->pgTable, faultaddress, &inText);
		paddr = entry->paddr;
	} else if (faultaddress >= vbase_data && faultaddress < vtop_data) {
		entry = getPTE(as->pgTable, faultaddress, &inText);
		paddr = entry->paddr;
	} else if (faultaddress >= stackbase && faultaddress < stacktop) {
		entry = getPTE(as->pgTable, faultaddress, &inText);
		paddr = entry->paddr;
	} else {
		return EFAULT;
	}

	//getPTE encountered a vaddr outside of segment range
	if (entry == NULL) {
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
		elo = inText? (paddr | TLBLO_VALID) & ~TLBLO_DIRTY : paddr | TLBLO_DIRTY | TLBLO_VALID;
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

