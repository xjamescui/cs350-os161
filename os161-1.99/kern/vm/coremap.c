#include <coremap.h>
#include <lib.h>
#include <mips/vm.h>
#include <vm.h>

#include "opt-A3.h"

#if OPT_A3

/**
 * find an available page in coremap and allocate it for use
 */
paddr_t alloc_page(void) {

	//FIFO algo
	if (vmInitialized) {
		lock_acquire(coremapLock);

		//add stuff here
		for (int a = 0; a < NUM_PAGES; a++) {
			if (coremap[a].state == FREE) {
				//do something

				return coremap[a].vaddr;
			}
		}
		lock_release(coremapLock);
	}

	return -1;
}

/**
 * allocate npages of avaialable pages in the coremap
 */
paddr_t alloc_pages(int npages) {

	(void) npages;

	if (vmInitialized) {
		lock_acquire(coremapLock);

		//add stuff here
		lock_release(coremapLock);
	}
	return -1;
}

void free_page(vaddr_t addr) {

	(void) addr;

	if (vmInitialized == 1) {
		lock_acquire(coremapLock);

		//add stuff here
		lock_release(coremapLock);
	}
}


#endif
