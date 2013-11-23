#include <types.h>

struct pte {
	int frameNum;
	//add protection flags here
	bool valid;
	bool readOnly;
};

struct pt {

	paddr_t pt_base;	//the base physical address
	struct pte * table;
};

void initPT (void);

int getPTE(vaddr_t addr);

/*

replacePage?

makePageAvail?

*/
