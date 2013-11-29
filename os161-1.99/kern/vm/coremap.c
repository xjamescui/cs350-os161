#include <coremap.h>
#include <lib.h>
#include <mips/vm.h>
#include <vm.h>
#include <synch.h>

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
paddr_t cm_alloc_pages(unsigned long npages) {

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

				for (int b = a - counter + 1; b < a + 1; b++) {
					// coremap[b].state = DIRTY;
					coremap[b].state = CLEAN; //should be clean instead
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

void free_page(vaddr_t addr) {

	(void) addr;

	if (vmInitialized == 1) {
		lock_acquire(coremapLock);

		//add stuff here
		lock_release(coremapLock);
	}
}

int getVictimIndex(void) {
	unsigned int min = 0xFFFFFFFF;
	int indexOfMin;

	for (int a = 0; a < NUM_PAGES; a++) {
		if (coremap[a].state == FIXED) {
			continue;
		} else {
			if (coremap[a].id < min) {
				min = coremap[a].id;
				indexOfMin = a;
			}
		}
	}

	return indexOfMin;
}

void printCM() {
	for (int a = 0; a < NUM_PAGES; a++) {

		kprintf(
				"a = %d, PAGE_SIZE= %d, coremap[%d].state=%d, address =%x, paddr = %x, blocksAlloc = %d\n",
				a, PAGE_SIZE, a, coremap[a].state, coremap[a].vaddr,
				coremap[a].paddr, coremap[a].pagesAllocated);
	}
}

#endif
