#include <pt.h>

void initPT(struct pt * pageTable) {
	//find out how many pages the proc needs
	
	//create a pt

	//for every page that it needs, create a pte and init appropriate fields
}

paddr_t getPTE(vaddr_t addr) {
	
	vaddr_t textBegin;
	vaddr_t textEnd;
	vaddr_t dataBegin;
	vaddr_t dataEnd;
	
	int vpn;	
	//initialize the above variables

	//boundary checking?

	//not in either segments
	if(!((textBegin <= addr && addr <= textEnd) || (dataBegin <= addr && addr <= dataEnd))) {
		//kill process
		return (paddr_t)-1;
	}
	//in text segment
	else if(textBegin <= addr && addr <= textEnd) {
		vpn = ((addr - textBegin) & 0xfffff000)/ PAGE_SIZE;

		if(text[vpn]->valid) {
			//add mapping to tlb, evict victim if necessary
			//reexecute instruction
			return text[vpn]->paddr;
		}
		else {
			//page fault
			//get from swapfile or elf file
		}
	}
	//in data segment
	//else if (dataBegin <= addr && addr <= dataEnd) {
	else {
		vpn = ((addr - dataBegin) & 0xfffff000)/ PAGE_SIZE;

		if(data[vpn]->valid) {
			//add mapping to tlb, evict victim if necessary
			//reexecute instruction
			return data[vpn]->paddr;
		}
		else {
			//page fault
		}
	}

	//for every pte, check if page number * pagesize <= addr <= page number +1 * pagesize
	(void) addr;
	return 0; //change this later
}
