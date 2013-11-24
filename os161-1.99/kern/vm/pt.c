#include <pt.h>
#include <mips/vm.h>

void initPT(struct pt * pageTable) {
	(void) pageTable;
	//find out how many pages the proc needs

	//create a pt

	//for every page that it needs, create a pte and init appropriate fields
}

paddr_t getPTE(struct pt* pageTable, vaddr_t addr) {

	//text segment
	vaddr_t textBegin;
	vaddr_t textEnd;

	//data segment
	vaddr_t dataBegin;
	vaddr_t dataEnd;

	int vpn;
	//initialize the above variables

	//boundary checking?

	//not in either segments
	if (!((textBegin <= addr && addr <= textEnd)
			|| (dataBegin <= addr && addr <= dataEnd))) {
		//kill process
		return (paddr_t) -1;
	}

	//in text segment
	else if (textBegin <= addr && addr <= textEnd) {
		vpn = ((addr - textBegin) & PAGE_FRAME) / PAGE_SIZE;

		if (pageTable->text[vpn]->valid) {
			//add mapping to tlb, evict victim if necessary
			//reexecute instruction
			return pageTable->text[vpn]->paddr;
		} else {
			//page fault
			//get from swapfile or elf file
		}
	}
	//in data segment
	else if (dataBegin <= addr && addr <= dataEnd) {
		vpn = ((addr - dataBegin) & PAGE_FRAME) / PAGE_SIZE;

		if (pageTable->data[vpn]->valid) {
			//add mapping to tlb, evict victim if necessary
			//reexecute instruction
			return pageTable->data[vpn]->paddr;
		} else {
			//page fault
		}
	}

	return 0; //change this later
}
