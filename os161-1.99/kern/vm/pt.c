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

//	int numTextPages = curproc_getas()->as_npages_text;
//	int numDataPages = curproc_getas()->as_npages_data;
//	const int numStackPages = 12;
//	(void) numStackPages;

	//create a pt
//	pgTable = kmalloc(sizeof(struct pt));

	//should we use kmalloc?
	pgTable->text = array_create();
	pgTable->data = array_create();

	//should we have one for the stack? what's going on here?
	//pageTable->text = (struct pte *) kmalloc (12 * sizeof(struct pte));

	//for every page that it needs, create a pte and init appropriate fields

//	for (int a = 0; a < numTextPages; a++) {
//		//load vaddr part only
//		pgTable->text[a] = kmalloc(sizeof(struct pte));
//		pgTable->text[a]->valid = false;
//		pgTable->text[a]->readOnly = false;
//		pgTable->text[a]->paddr = (paddr_t) NULL;
//	}
//
//	for (int a = 0; a < numDataPages; a++) {
//		pgTable->data[a] = kmalloc(sizeof(struct pte));
//		pgTable->data[a]->valid = false;
//		pgTable->data[a]->readOnly = false;
//		pgTable->data[a]->paddr = (paddr_t) NULL;
//
//	}
}

paddr_t getPTE(struct pt* pgTable, vaddr_t addr) {

	int dbflags = DB_A3;

	//text segment
	vaddr_t textBegin = curproc_getas()->as_vbase_text;
	vaddr_t textEnd = textBegin + curproc_getas()->as_npages_text * PAGE_SIZE;

	KASSERT(textEnd > textBegin);

	//data segment
	vaddr_t dataBegin = curproc_getas()->as_vbase_data;
	vaddr_t dataEnd = dataBegin + curproc_getas()->as_npages_data * PAGE_SIZE;

	KASSERT(dataEnd > dataBegin);

	int vpn;
	//the entry we are looking into
	struct pte* entry = NULL;

	//not in either segments
	if (!((textBegin <= addr && addr <= textEnd)
			|| (dataBegin <= addr && addr <= dataEnd))) {
		//kill process
		return EFAULT;
	}

	//in text segment
	else if (textBegin <= addr && addr <= textEnd) {
		vpn = ((addr - textBegin) & PAGE_FRAME) / PAGE_SIZE;
		entry = (struct pte*)array_get(pgTable->text, vpn);
		if (entry->valid) {
			//add mapping to tlb, evict victim if necessary
			//reexecute instruction

			DEBUG(DB_A3, "addr is in text segment\n");

			return entry->paddr;
		} else {
			//page fault
			//get from swapfile or elf file
			DEBUG(DB_A3, "page fault: addr is in text segment \n");
			return loadPTE(pgTable, addr, 1, textBegin);
		}
	}
	//in data segment
	else if (dataBegin <= addr && addr <= dataEnd) {
		vpn = ((addr - dataBegin) & PAGE_FRAME) / PAGE_SIZE;
		entry = (struct pte*)array_get(pgTable->data, vpn);

		if (entry->valid) {
			//add mapping to tlb, evict victim if necessary
			//reexecute instruction
			DEBUG(DB_A3, "addr is in data segment\n");
			return entry->paddr;
		} else {
			DEBUG(DB_A3, "page fault: addr is in data segment \n");
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
		((struct pte*)array_get(pgTable->text, vpn))->paddr = paddr;
	} else {
		((struct pte*)array_get(pgTable->data, vpn))->paddr = paddr;
	}

	//something has gone wrong!
	return EFAULT;
}
