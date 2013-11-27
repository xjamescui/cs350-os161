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
#define DATA_SEG 0
#define TEXT_SEG 1
#define STACK_SEG 2

void initPT(struct pt * pgTable, int numTextPages, int numDataPages) {

	pgTable->numTextPages = numTextPages;
	pgTable->numDataPages = numDataPages;
	pgTable->text = kmalloc(numTextPages * sizeof(struct pte*));
	pgTable->data = kmalloc(numDataPages * sizeof(struct pte*));
	pgTable->stack = kmalloc(DUMBVM_STACKPAGES * sizeof(struct pte*));

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

	for (int a = 0; a < DUMBVM_STACKPAGES; a++) {
		pgTable->stack[a] = kmalloc(sizeof(struct pte));
		pgTable->stack[a]->valid = false;
		pgTable->stack[a]->readOnly = false;
		pgTable->stack[a]->paddr = (paddr_t) NULL;

	}
}

struct pte * getPTE(struct pt* pgTable, vaddr_t addr) {

	int dbflags = DB_A3;
	DEBUG(DB_A3, "getPTE is called: faultaddr=%x\n", addr);

	//text segment
	vaddr_t textBegin = curproc_getas()->as_vbase_text;
	vaddr_t textEnd = textBegin + curproc_getas()->as_npages_text * PAGE_SIZE;

	KASSERT(textEnd > textBegin);

	//data segment
	vaddr_t dataBegin = curproc_getas()->as_vbase_data;
	vaddr_t dataEnd = dataBegin + curproc_getas()->as_npages_data * PAGE_SIZE;

	vaddr_t stackTop = USERSTACK;
	vaddr_t stackBase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;

	KASSERT(dataEnd > dataBegin);

	//the entry we are looking into
	struct pte* entry = NULL;
	int vpn;

	//not in either segments
	if (!((textBegin <= addr && addr <= textEnd)
			|| (dataBegin <= addr && addr <= dataEnd)
			|| (stackBase <= addr && addr <= stackTop))) {
		//kill process
		DEBUG(DB_A3, "faultaddr: BAD area\n");
		return NULL;
	}

	//in text segment
	else if (textBegin <= addr && addr <= textEnd) {

		vpn = ((addr - textBegin) & PAGE_FRAME) / PAGE_SIZE;
		entry = pgTable->text[vpn];
		if (entry->valid) {
			//add mapping to tlb, evict victim if necessary
			//reexecute instruction
			KASSERT(entry != NULL);
			DEBUG(DB_A3, "addr is in text segment\n");
			return entry;
		} else {
			//page fault
			//get from swapfile or elf file
			DEBUG(DB_A3, "page fault: addr is in text segment, vpn =%d \n", vpn);
			return loadPTE(pgTable, addr, TEXT_SEG, textBegin);
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
			DEBUG(DB_A3, "page fault: addr is in data segment, vpn = %d \n", vpn);
			return loadPTE(pgTable, addr, DATA_SEG, dataBegin);
		}
	}
	//in stack segment
	else {
		vpn = ((addr - stackBase) & PAGE_FRAME) / PAGE_SIZE;
		entry = pgTable->stack[vpn];

		if (entry->valid) {
			//add mapping to tlb, evict victim if necessary
			//reexecute instruction
			DEBUG(DB_A3, "addr is in stack segment\n");
			return entry;
		} else {
			DEBUG(DB_A3, "page fault: addr is in stack segment, vpn=%d \n", vpn);
			return loadPTE(pgTable, addr, STACK_SEG, stackBase);
		}
	}

	// return EFAULT;
	return NULL;
}

struct pte * loadPTE(struct pt * pgTable, vaddr_t faultaddr,
		unsigned short int segmentNum, vaddr_t segBegin) {

	int dbflags = DB_A3;
	int result;
	struct iovec iov;
	struct uio u;
	off_t uio_offset;
	int vpn = ((faultaddr - segBegin) & PAGE_FRAME) / PAGE_SIZE;

	//we only need one page
	//paddr_t paddr = cm_alloc_pages(1);
	paddr_t paddr;
	long allocPageResult = cm_alloc_pages(1);
	if (allocPageResult > 0) {
		paddr = (paddr_t) allocPageResult;
		//ALERT: there may be a bug here
	}
	//Register this physical page in the core map
	//copy paddr over to page table with info from ELF/SWAP file

	//no more physical ram available
	if (allocPageResult == -1) {
		//need to check if all pages in page table are valid
		//if not we use the non valid page
		//if so, we need to swap out a page in pagetable
		//then update coremap
		kprintf("error in loadPTE\n");

		//NOTE: the swapping
	}

//	if (result) {
//		return NULL; //Some error
//	}

	DEBUG(DB_A3, "welcome to loadPTE\n");

	//load page based on swapfile or ELF file
//	iov.iov_kbase = (void *) PADDR_TO_KVADDR(paddr); //where we want to transfer to
//	iov.iov_len = PAGE_SIZE;  //length of the memory space
//	u.uio_iov = &iov;
//	u.uio_iovcnt = 1;
//	u.uio_resid = PAGE_SIZE;
//	u.uio_rw = UIO_READ;
//	u.uio_space = NULL;
//	u.uio_segflg = UIO_SYSSPACE;


	switch (segmentNum) {
	case TEXT_SEG:
		uio_offset = vpn*PAGE_SIZE + curproc->p_elf->elf_text_offset;
		break;
	case DATA_SEG:
		uio_offset = vpn*PAGE_SIZE + curproc->p_elf->elf_data_offset;
		break;
	default:
		break;
	}


	//READ from ELF if we are not dealing with something on the stack
	uio_kinit(&iov, &u, (void *) PADDR_TO_KVADDR(paddr), PAGE_SIZE, uio_offset,
			UIO_READ);
	if (segmentNum != STACK_SEG) {

		result = VOP_READ(curproc->p_elf->v, &u);
		if (result) {
			DEBUG(DB_A3, "VOP_READ ERROR!!\n");
			return NULL;
		}

		if (u.uio_resid != 0) {
			/* short read; problem with executable? */
			kprintf("ELF: short read on segment - file truncated?\n");
			return NULL;
		}
	}
	else{
		//for stack
		uiomovezeros(PAGE_SIZE, &u);
	}
	DEBUG(DB_A3, "welcome to loadPTE: read finished\n");
//Register this physical page in the core map
	if (segmentNum == TEXT_SEG) {
		pgTable->text[vpn]->vaddr = faultaddr;
		pgTable->text[vpn]->paddr = paddr;
		pgTable->text[vpn]->valid = 1;
		return pgTable->text[vpn];
	} else if (segmentNum == DATA_SEG) {
		pgTable->data[vpn]->vaddr = faultaddr;
		pgTable->data[vpn]->paddr = paddr;
		pgTable->data[vpn]->valid = 1;
		return pgTable->data[vpn];
	} else if (segmentNum == STACK_SEG) {
		pgTable->stack[vpn]->vaddr = faultaddr;
		pgTable->stack[vpn]->paddr = paddr;
		pgTable->stack[vpn]->valid = 1;
		return pgTable->stack[vpn];
	} else {
		//something wrong!
		return NULL;
	}

//something has gone wrong!
	return NULL;
}

void printPT(struct pt* pgTable) {

	kprintf("----Printing Page Table---------\n");

	for (unsigned int a = 0; a < curproc_getas()->as_npages_text; a++) {
		//load vaddr part only

		kprintf("textPTE[%d] vaddr: %x, paddr: %x, valid: %d\n", a,
				pgTable->text[a]->vaddr, pgTable->text[a]->paddr,
				pgTable->text[a]->valid);
	}

	for (unsigned int a = 0; a < curproc_getas()->as_npages_data; a++) {
		kprintf("dataPTE[%d] vaddr: %x, paddr: %x, valid: %d\n", a,
				pgTable->data[a]->vaddr, pgTable->data[a]->paddr,
				pgTable->data[a]->valid);

	}

	for (unsigned int a = 0; a < DUMBVM_STACKPAGES; a++) {
		kprintf("stackPTE[%d] vaddr: %x, paddr: %x, valid: %d\n", a,
				pgTable->stack[a]->vaddr, pgTable->stack[a]->paddr,
				pgTable->stack[a]->valid);
	}

}

//We use a random page replacement algorithm, as it is actually faster than FIFO in practise.
//it is also by far the simplest replacement algorithm to use as we needn't keep track of the order of pages
//inserted into the page table like FIFO needs.
//NOTE page replacement will  
int getVictimVPN(struct pt * pgTable, unsigned short int segmentNum) {
	if (segmentNum == TEXT_SEG) {
		return random() % pgTable->numTextPages;
	} else if (segmentNum == DATA_SEG) {
		return random() % pgTable->numDataPages;
	} else if (segmentNum == STACK_SEG) {
		return random() % DUMBVM_STACKPAGES;
	} else {
		//we shouldn't get here
		return -1;
	}
}
