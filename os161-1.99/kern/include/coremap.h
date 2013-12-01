#ifndef _COREMAP_H_
#define _COREMAP_H_
#include <types.h>

//alloc methods
paddr_t cm_alloc_pages(unsigned long npages, bool inKernel);
void free_page(paddr_t addr, bool inKernel);
int getVictimIndex(void);
void printCM(void);

#define FREE 0  //page is available and is unalloc'ed
#define HOGGED 1 	//page is alloc'ed and belongs to important owners (ie kernel), can't be used by others
#define CLEAN 2 //page is alloc'ed and used and has not been modified
#define DIRTY 3 //page is alloc'ed and used and has been modified

/* a physical page (frame) for coremap */
struct page {
	struct addrspace * as;

	//the vaddr the frame is mapped to
	volatile vaddr_t vaddr;
	volatile paddr_t paddr;

	/**
	 * n for n consecutive blocks allocated and this is the first block
	 * -1 otherwise
	 **/
	volatile int pagesAllocated;

	/**
	 * 0 = free
	 * 1 = hogged
	 * 2 = clean
	 * 3 = dirty
	 **/
	volatile int state;

	//this variable is used in the page replacement algorithm
	//a global variable is used to assign ids in sequential fashion
	//and the variable simply choose the smallest id that meets criteria
	//to evict.
	//currently, ids are not reused and will cause an overflow when we hit 2^32
	volatile unsigned int id;
};

struct lock * coremapLock;
struct page * coremap;
#endif
