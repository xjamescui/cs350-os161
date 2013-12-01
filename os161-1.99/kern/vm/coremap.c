#include <types.h>
#include <coremap.h>
#include <lib.h>
#include <mips/vm.h>
#include <vm.h>
#include <synch.h>
#include <proc.h>
#include "opt-A3.h"

#if OPT_A3
#include <addrspace.h>
//we assign this to the id of the page allocated
unsigned int idCounter = 1;

/**
 * allocate npages of available pages in the coremap
 * - synchronized
 * I think currently, we  only call this with param: 1
 * let's keep it that way, I don't think allocating multiple pages may
 * work with some of the stuff we've got here
 */
paddr_t cm_alloc_pages(unsigned long npages, bool inKernel) {

	if (vmInitialized) {
		lock_acquire(coremapLock);

		unsigned long counter = 0;
		KASSERT(NUM_PAGES >= 0);

		for (int a = 0; a < NUM_PAGES; a++) {
			if (coremap[a].state == FREE) {
				counter++;
			} else {
				counter = 0;
			}
			if (counter == npages) {
				coremap[a - counter + 1].pagesAllocated = (int) npages;
				coremap[a - counter + 1].id = idCounter;
				idCounter++;

				if (idCounter == 0xFFFFFFFF) {
					//we've set the max id, reset all ids to zero
					for (int i = 0; i < NUM_PAGES; i++) {
						coremap[i].id = 0;
					}
					idCounter = 1;
				}

    			struct addrspace *as = curproc_getas();
				for (int b = a - counter + 1; b < a + 1; b++) {
					// coremap[b].state = DIRTY;
					if(inKernel) {
						coremap[b].state = HOGGED;
					}
					else {
						coremap[b].state = CLEAN; //should be clean instead
     					coremap[b].as = as;
 					}
				}

				//zero out
				as_zero_region(coremap[a- counter + 1].paddr,npages);
				lock_release(coremapLock);
				return (paddr_t) coremap[a - counter + 1].paddr;
				//set pages to be used
			}
		}

		lock_release(coremapLock);
	}
	return -1; //cannot get continuous blocks of avaialable pages
}

void free_page(paddr_t paddr, bool inKernel) {
	if (vmInitialized == 1) {
		lock_acquire(coremapLock);

		int startingIndex = paddr / PAGE_SIZE;
		int pagesToFree = coremap[startingIndex].pagesAllocated;

		KASSERT(pagesToFree > 0);

		for (int b = startingIndex; b < startingIndex+ pagesToFree; b++) {
			//need to make sure state is not hogged
			if (inKernel || coremap[b].state != HOGGED) {
				// kprintf("FREEING INDEX=%d\n", b);
				// coremap[b].paddr = (paddr_t) NULL;
				coremap[b].state = FREE;
    			coremap[b].as = NULL;
	  			coremap[b].pagesAllocated = -1;
	  			coremap[b].vaddr = 0;
				//addr needed to be aligned by 4k
			}
		}
		as_zero_region(coremap[startingIndex].paddr,pagesToFree);



		lock_release(coremapLock);
	}
	else {
		panic("kernel tried to free a page before vm is initialized?");
	}
}

//FIFO algorithm, returns the page with the lowest id that's not fixed
int getVictimIndex(void) {

	//add locks?
	lock_acquire(coremapLock);
	unsigned int min = 0xFFFFFFFF;
	int indexOfMin;
 //Is there any pages that we shouldn't use? Such as kernel pages.
	for (int a = 0; a < NUM_PAGES; a++) {
		if (coremap[a].state == HOGGED) {
			continue;
		} else {
			if (coremap[a].id < min) {
				min = coremap[a].id;
				indexOfMin = a;
			}
		}
	}
	lock_release(coremapLock);
	return indexOfMin;
}

void printCM() {
	for (int a = 0; a < NUM_PAGES; a++) {

		kprintf(
				"a = %d, PAGE_SIZE= %d, coremap[%d].state=%d, address =%x, paddr = %x, blocksAlloc = %d\n",
				a, PAGE_SIZE, a, coremap[a].state, coremap[a].vaddr,
				coremap[a].paddr, coremap[a].pagesAllocated);
	

struct addrspace * as = coremap[a].as;
 if (as != NULL) {
 kprintf("vbase_text: %x pbase_text: %x npages_text : %d vbase_data: %x pbase_data %x npages_data %d as_stackpbase %x\n\n", as->as_vbase_text, as->as_pbase_text, as->as_npages_text, as->as_vbase_data, as->as_pbase_data, as->as_npages_data, as->as_stackpbase);
 }
}

}

#endif
