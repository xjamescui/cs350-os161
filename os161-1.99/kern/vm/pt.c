#include <types.h>
#include <lib.h>
#include <pt.h>
#include <proc.h>
#include <addrspace.h>
#include <mips/vm.h>
#include <kern/errno.h>
#include <coremap.h>

void initPT(struct pt * pgTable) {
	//find out how many pages the proc needs

	int numTextPages;
	int numDataPages;
	const int numStackPages = 12;
	(void) numStackPages;

	//create a pt

	//should we use kmalloc?
	pgTable->text = kmalloc(numTextPages * sizeof(struct pte));
	pgTable->data = kmalloc(numDataPages * sizeof(struct pte));

	//should we have one for the stack? what's going on here?
	//pageTable->text = (struct pte *) kmalloc (12 * sizeof(struct pte));

	//for every page that it needs, create a pte and init appropriate fields
	for (int a = 0; a < numTextPages; a++) {
		//init the ptes here
	}

	for (int a = 0; a < numDataPages; a++) {
		//init the ptes here
	}
}

paddr_t getPTE(struct pt* pgTable, vaddr_t addr) {

	KASSERT(curproc_getas() != NULL);

	//text segment
	vaddr_t textBegin = curproc_getas()->as_vbase_text;
	vaddr_t textEnd = textBegin + curproc_getas()->as_npages_text * PAGE_SIZE;

	KASSERT(textEnd > textBegin);

	//data segment
	vaddr_t dataBegin = curproc_getas()->as_vbase_data;
	vaddr_t dataEnd = dataBegin + curproc_getas()->as_npages_data * PAGE_SIZE;

	KASSERT(dataEnd > dataBegin);

	int vpn;

	//not in either segments
	if (!((textBegin <= addr && addr <= textEnd)
			|| (dataBegin <= addr && addr <= dataEnd))) {
		//kill process
		return EFAULT;
	}

	//in text segment
	else if (textBegin <= addr && addr <= textEnd) {
		vpn = ((addr - textBegin) & PAGE_FRAME) / PAGE_SIZE;

		if (pgTable->text[vpn]->valid) {
			//add mapping to tlb, evict victim if necessary
			//reexecute instruction
			return pgTable->text[vpn]->paddr;
		} else {
			//page fault
			//get from swapfile or elf file
			return loadPTE(pgTable, addr, 1, textBegin);
		}
	}
	//in data segment
	else if (dataBegin <= addr && addr <= dataEnd) {
		vpn = ((addr - dataBegin) & PAGE_FRAME) / PAGE_SIZE;

		if (pgTable->data[vpn]->valid) {
			//add mapping to tlb, evict victim if necessary
			//reexecute instruction
			return pgTable->data[vpn]->paddr;
		} else {
			return loadPTE(pgTable, addr, 0, dataBegin);
		}
	}

	return EFAULT;
}

paddr_t loadPTE(struct pt * pgTable, vaddr_t vaddr, bool inText,
		vaddr_t segBegin) {
	//load page based on swapfile or ELF file
	//using cm_alloc_pages

	//we only need one page
	paddr_t paddr = cm_alloc_pages(1);



	//copy paddr over to page table
	int vpn = ((vaddr - segBegin) & PAGE_FRAME) / PAGE_SIZE;

	if (inText) {
		pgTable->text[vpn]->paddr = paddr;
	} else {
		pgTable->data[vpn]->paddr = paddr;
	}

	//something has gone wrong!
	return EFAULT;
}
