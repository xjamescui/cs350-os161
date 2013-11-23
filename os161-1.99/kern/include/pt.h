struct pte {
	int frameNum;
	//add proctection flags here
	bool valid;
	bool readOnly;
}

struct pt {
	//the base physical address
	paddr_t base;	
	struct pte * table;
}

void initPT (void);

int getPTE(vaddr_t addr);

/*

replacePage?

makePageAvail?

*/