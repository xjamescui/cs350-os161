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
	unsigned int numTextPages; //we store the number of pages required for each segment as defined in the ELF file
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

//Action for when there's not enough physical memory
paddr_t noPhysicalMemoryAction(struct pt * pgTable, vaddr_t faultaddr, unsigned short int segmentNum, vaddr_t segBegin, vaddr_t segEnd, int vpn);

#endif
