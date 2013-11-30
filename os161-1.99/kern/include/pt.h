#ifndef _PT_H_
#define _PT_H_

#include <types.h>

#define DATA_SEG 0
#define TEXT_SEG 1
#define STACK_SEG 2

//page table entry struct
struct pte {
	vaddr_t vaddr;
	paddr_t paddr;
	//add protection flags here
	bool valid;
	bool readOnly;
};

//page table struct
struct pt {
	unsigned int numTextPages;
	unsigned int numDataPages;

	//have three "mini" page tables for each segment for faster searching
	struct pte** text;
	struct pte** data;
	struct pte** stack;
};

//initialize the page table
void initPT(struct pt * pgTable, int numTextPages, int numDataPages);

//find an entry in the page table
struct pte * getPTE(struct pt * pgTable, vaddr_t addr, int* inText);

// this function is called when there is a page fault, and we need to get the corresponding page from swapfile or ELF file
struct pte * loadPTE(struct pt * pgTable, vaddr_t vaddr, unsigned short int segmentNum,
		vaddr_t segBegin, vaddr_t segEnd);

int destroyPT(struct pt * pgTable);


//print the page table to standard output
void printPT(struct pt* pgTable);


//We use a random page replacement algorithm, as it is actually faster than FIFO in practise.
//it is also by far the simplest replacement algorithm to use as we needn't keep track of the order of pages
//inserted into the page table like FIFO needs.
//int getVictimVPN(struct pt * pgTable, unsigned short int segmentNum);
/* vaddr_t to paddr_t */

/*

 replacePage?

 makePageAvail?

 */
#endif
