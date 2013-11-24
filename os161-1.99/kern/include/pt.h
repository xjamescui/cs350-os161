#include <types.h>

struct pte {
	paddr_t paddr;
	//add protection flags here
	bool valid;
	bool readOnly;
};

struct pt {
	//have two "mini" page tables for each segment
	struct pte ** text;
	struct pte ** data;
};

void initPT(struct pt * pageTable);

paddr_t getPTE(struct pt * pageTable, vaddr_t addr);

/* vaddr_t to paddr_t */

/*

 replacePage?

 makePageAvail?

 */

