#include <types.h>
#include <lib.h>
#include <pt.h>
#include <proc.h>
#include <addrspace.h>
#include <mips/vm.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <coremap.h>
#include <elf.h>
#include <uio.h>
#include <vnode.h>
#include <current.h>
#include <vfs.h>

#define DUMBVM_STACKPAGES    12

void initPT(struct pt * pgTable, int numTextPages, int numDataPages) {

//	const int numStackPages = 12;
//	(void) numStackPages;
	pgTable->numTextPages = numTextPages;
	pgTable->numDataPages = numDataPages;
	pgTable->text = kmalloc(numTextPages * sizeof(struct pte*));
	pgTable->data = kmalloc(numDataPages * sizeof(struct pte*));

	//should we have one for the stack? what's going on here?
	//pageTable->text = (struct pte *) kmalloc (12 * sizeof(struct pte));

	//for every page that it needs, create a pte and init appropriate fields

	for (int a = 0; a < numTextPages; a++) {
		//load vaddr part only
		pgTable->text[a] = kmalloc(sizeof(struct pte));
		pgTable->text[a]->valid = false;
		pgTable->text[a]->readOnly = false;
		pgTable->text[a]->paddr = (paddr_t) NULL;
	}

	for (int a = 0; a < numDataPages; a++) {
		pgTable->data[a] = kmalloc(sizeof(struct pte));
		pgTable->data[a]->valid = false;
		pgTable->data[a]->readOnly = false;
		pgTable->data[a]->paddr = (paddr_t) NULL;

	}
}

struct pte * getPTE(struct pt* pgTable, vaddr_t addr) {

	int dbflags = DB_A3;
	DEBUG(DB_A3, "getPTE is called\n");

	//text segment
	vaddr_t textBegin = curproc_getas()->as_vbase_text;
	vaddr_t textEnd = textBegin + curproc_getas()->as_npages_text * PAGE_SIZE;

	KASSERT(textEnd > textBegin);

	//data segment
	vaddr_t dataBegin = curproc_getas()->as_vbase_data;
	vaddr_t dataEnd = dataBegin + curproc_getas()->as_npages_data * PAGE_SIZE;

	vaddr_t stackBegin; //this is also constant, i think
	vaddr_t stackEnd; //this is constant, i think
	(void)stackEnd; //do we need stackEnd?

	KASSERT(dataEnd > dataBegin);

	int vpn;
	//the entry we are looking into
	struct pte* entry = NULL;

	//not in either segments
	if (!((textBegin <= addr && addr <= textEnd)
			|| (dataBegin <= addr && addr <= dataEnd))) {
		//kill process
		return NULL;
	}

	//in text segment
	else if (textBegin <= addr && addr <= textEnd) {
		vpn = ((addr - textBegin) & PAGE_FRAME) / PAGE_SIZE;
		entry = pgTable->text[vpn];
		if (entry->valid) {
			//add mapping to tlb, evict victim if necessary
			//reexecute instruction

			DEBUG(DB_A3, "addr is in text segment\n");

			return entry;
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
		entry = pgTable->data[vpn];

		if (entry->valid) {
			//add mapping to tlb, evict victim if necessary
			//reexecute instruction
			DEBUG(DB_A3, "addr is in data segment\n");
			return entry;
		} else {
			DEBUG(DB_A3, "page fault: addr is in data segment \n");
			return loadPTE(pgTable, addr, 0, dataBegin);
		}
	}
	//in stack segment
	else {
		vpn = ((addr - stackBegin) & PAGE_FRAME) / PAGE_SIZE;
		entry = pgTable->stack[vpn];

		if (entry->valid) {
			//add mapping to tlb, evict victim if necessary
			//reexecute instruction
			DEBUG(DB_A3, "addr is in stack segment\n");
			return entry;
		} else {
			DEBUG(DB_A3, "page fault: addr is in stack segment \n");
			return loadPTE(pgTable, addr, 2, dataBegin);
		}
	}

	// return EFAULT;
	return NULL;
}

struct pte * loadPTE(struct pt * pgTable, vaddr_t vaddr, unsigned short int segmentNum,
		vaddr_t segBegin) {
	//load page based on swapfile or ELF file
 struct vnode *v;
 int result;
 Elf_Ehdr eh; /* Executable header */
 Elf_Phdr ph; /* "Program header" = segment header */
 struct iovec iov;
 struct uio ku;

 //change this later
 (void)eh;
 (void)ph;
 (void)iov;
 (void)ku;
 (void)v;

 result = vfs_open(curproc->elf_name, O_RDONLY, 0, &v);
  if (result) {
   return NULL; //Some error
 }

	//using cm_alloc_pages

	//we only need one page
	//paddr_t paddr = cm_alloc_pages(1); 
 	paddr_t paddr;
	long allocPageResult = cm_alloc_pages(1);
	if(allocPageResult > 0) {
		paddr = (paddr_t)allocPageResult;
		//ALERT: there may be a bug here
	}
    //Register this physical page in the core map
	//copy paddr over to page table with info from ELF/SWAP file
	
	int vpn = ((vaddr - segBegin) & PAGE_FRAME) / PAGE_SIZE;

	//no more physical ram available
	if(allocPageResult == -1) {
		//need to check if all pages in page table are valid
		//if not we use the non valid page
		//if so, we need to swap out a page in pagetable
		//then update coremap

		//NOTE: the swapping 
	}

	if (segmentNum == 0) {
		pgTable->text[vpn]->paddr = paddr;

		return pgTable->text[vpn];
	} 
	else if (segmentNum == 1) {
		pgTable->data[vpn]->paddr = paddr;

		return pgTable->text[vpn];
	}
	else if(segmentNum == 2) {
		pgTable->stack[vpn]->paddr = paddr;
		return pgTable->text[vpn];
	}
	else {
		//something wrong!
		return NULL;
	}

	//something has gone wrong!
	// return EFAULT;
	return NULL;
}

//We use a random page replacement algorithm, as it is actually faster than FIFO in practise.
//it is also by far the simplest replacement algorithm to use as we needn't keep track of the order of pages
//inserted into the page table like FIFO needs.
//NOTE page replacement will  
int getVictimVPN(struct pt * pgTable, unsigned short int segmentNum) {
	//JAMES, PLEASE DEFINE MACROS FOR THESE
	//text segment = 0, data = 1, stack = 2
	if(segmentNum == 0) {
		return random() % pgTable->numTextPages;
	}
	else if(segmentNum == 1) {
		return random() % pgTable->numDataPages;
	}
	else if (segmentNum == 2) {
		return random() % DUMBVM_STACKPAGES;
	}
	else {
		//we shouldn't get here
		return -1;
	}
}
