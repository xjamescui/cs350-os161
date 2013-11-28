#include <coremap.h>
#include <lib.h>
#include <mips/vm.h>
#include <vm.h>
#include <synch.h>

#include "opt-A3.h"

#if OPT_A3

/**
 * allocate npages of available pages in the coremap
 * - synchronized
 */
paddr_t cm_alloc_pages(unsigned long npages) {

	if (vmInitialized) {
		lock_acquire(coremapLock);

		unsigned long counter = 0;
		KASSERT(NUM_PAGES >= 0);

		for (int a = 0; a < NUM_PAGES; a++) {
			if (coremap[a].state == FREE || coremap[a].state == CLEAN) {
				counter++;
			} else {
				counter = 0;
			}
			if (counter == npages) {
				coremap[a - counter + 1].blocksAllocated = (int) npages;
				for (int b = a - counter + 1; b < a + 1; b++) {
					coremap[b].state = DIRTY;
				}
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

int getVictimIndex() {
	//we need to change this, this will give us the first page that is not fixed everytime, which in a lot of cases is teh same page.
	for(int a = 0 ; a < NUM_PAGES ; a++) {
		if(coremap[a]->state == FIXED) {
			continue;
		}
		else {
			return a;
		}
	}
}

#endif
