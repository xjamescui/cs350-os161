#include <types.h>

struct vnode;

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

void initPT(struct pt * pgTable);

paddr_t getPTE(struct pt * pgTable, vaddr_t addr);


// this function is called when there is a page fault, and we need to get the corresponding page from swapfile or ELF file
// the boolean is 1 if vaddr is in text segment, 0 if it's in the only other segment (data)
// segBegin thus corresponds with the beginning vaddr of the segment define by inText
paddr_t loadPTE(struct pt * pgTable, vaddr_t vaddr, bool inText, vaddr_t segBegin);

/* vaddr_t to paddr_t */

int elf_to_ram(struct vnode* v, vaddr_t vaddr, paddr_t paddr);

/*

 replacePage?

 makePageAvail?

 */

