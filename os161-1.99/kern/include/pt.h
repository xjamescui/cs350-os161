#include <types.h>

struct pte {
	vaddr_t vaddr;
	paddr_t paddr;
	//add protection flags here
	bool valid;
	bool readOnly;
};

struct pt {
	//have two "mini" page tables for each segment
	unsigned int numTextPages;
	unsigned int numDataPages;
	struct pte** text;
	struct pte** data;
	struct pte** stack;
};

void initPT(struct pt * pgTable, int numTextPages, int numDataPages);

struct pte * getPTE(struct pt * pgTable, vaddr_t addr);

// this function is called when there is a page fault, and we need to get the corresponding page from swapfile or ELF file
// the unsigned short int is 0 if text seg, 1 if data seg, and 2 if stack seg.
// segBegin thus corresponds with the beginning vaddr of the segment define by inText
struct pte * loadPTE(struct pt * pgTable, vaddr_t vaddr, unsigned short int segmentNum,
		vaddr_t segBegin);

void printPT(struct pt* pgTable);


//We use a random page replacement algorithm, as it is actually faster than FIFO in practise.
//it is also by far the simplest replacement algorithm to use as we needn't keep track of the order of pages
//inserted into the page table like FIFO needs.
int getVictimVPN(struct pt * pgTable, unsigned short int segmentNum);
/* vaddr_t to paddr_t */

/*

 replacePage?

 makePageAvail?

 */

