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
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

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

struct pte * getPTE(struct pt* pgTable, vaddr_t addr, int* inText) {

//	int dbflags = DB_A3;
	DEBUG(DB_A3, "getPTE is called: faultaddr=%x\n", addr);

	//text segment
	vaddr_t textBegin = curproc_getas()->as_vbase_text;
	vaddr_t textEnd = textBegin + curproc_getas()->as_npages_text * PAGE_SIZE;

//	kprintf("text_seg_begin : 0x%x\n", textBegin);
//	kprintf("text_seg_end : 0x%x\n", textEnd),

	KASSERT(textEnd > textBegin);

	//data segment
	vaddr_t dataBegin = curproc_getas()->as_vbase_data;
	vaddr_t dataEnd = dataBegin + curproc_getas()->as_npages_data * PAGE_SIZE;

//	kprintf("data_seg_begin : 0x%x\n", dataBegin);
//	kprintf("data_seg_end : 0x%x\n", dataEnd);

	vaddr_t stackTop = USERSTACK;
	vaddr_t stackBase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;

//	kprintf("stack_seg_begin : 0x%x\n", stackBase);
//	kprintf("stack_seg_end : 0x%x\n", stackTop);

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

		*inText = 1;
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
			DEBUG(DB_A3, "page fault: addr is in text segment, vpn =%d \n",
					vpn);
			return loadPTE(pgTable, addr, TEXT_SEG, textBegin, textEnd);
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
			DEBUG(DB_A3, "page fault: addr is in data segment, vpn = %d \n",
					vpn);
			return loadPTE(pgTable, addr, DATA_SEG, dataBegin, dataEnd);
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
			DEBUG(DB_A3, "page fault: addr is in stack segment, vpn=%d \n",
					vpn);
			return loadPTE(pgTable, addr, STACK_SEG, stackBase, stackTop);
		}
	}

	// return EFAULT;
	return NULL;
}

struct pte * loadPTE(struct pt * pgTable, vaddr_t faultaddr,
		unsigned short int segmentNum, vaddr_t segBegin, vaddr_t segEnd) {
	(void) segEnd;
	int result;
	struct iovec iov;
	struct uio u;
	off_t uio_offset;
	size_t loadsize;
	int vpn = ((faultaddr - segBegin) & PAGE_FRAME) / PAGE_SIZE;

	uint32_t textFileBegin = curproc->p_elf->elf_text_offset;
	uint32_t textFileEnd = textFileBegin + curproc->p_elf->elf_text_filesz;

	uint32_t dataFileBegin = curproc->p_elf->elf_data_offset;
	uint32_t dataFileEnd = dataFileBegin + curproc->p_elf->elf_data_filesz;

	uint32_t fileBegin =
			(segmentNum == TEXT_SEG) ? textFileBegin : dataFileBegin;
	uint32_t fileEnd = (segmentNum == TEXT_SEG) ? textFileEnd : dataFileEnd;

	/*
	 Page fault
	 if no mem
	 load new page from swap, if can't find, find in elf ***
	 get victim in CM
	 swap out victim, invalidate pt entry
	 evict victim from CM
	 load new page into evicted slot
	 update pt and CM***
	 return paddr_t***
	 if mem avail
	 load new page from elf***
	 update pt and CM***
	 return paddr_t***
	 */

	//we only need one page
	//paddr_t paddr = cm_alloc_pages(1);
	paddr_t paddr;
	long allocPageResult = getppages(1);
	if (allocPageResult > 0) {
		paddr = (paddr_t) allocPageResult;
		//ALERT: there may be a bug here
	}
	//Register this physical page in the core map
	//copy paddr over to page table with info from ELF/SWAP file

	//no more physical ram available
	if (allocPageResult == -1) {
		//we ran out of physical memory
		//need to find a victim in coremap to evict and
		//invalidate the corresponding pte in page table of the process
		//owning the page

		kprintf("error in loadPTE\n");
//		printCM();
		return NULL;
		//NOTE: the swapping
	} else {
		// we can get a physical page, we haven't run out of memory yet.

	}

	//load page based on swapfile or ELF file
	KASSERT(faultaddr < segEnd);
	switch (segmentNum) {
	case TEXT_SEG:
		uio_offset = vpn * PAGE_SIZE + textFileBegin;
		break;
	case DATA_SEG:
		uio_offset = vpn * PAGE_SIZE + dataFileBegin;
		break;
	default:
		break;
	}

	//zero out the entire page first before we use it----------
	uio_kinit(&iov, &u, (void *) PADDR_TO_KVADDR(paddr), PAGE_SIZE, uio_offset,
			UIO_READ);
	uiomovezeros(PAGE_SIZE, &u);
	//end of: zero out---------------------

	if (segmentNum != STACK_SEG) {
		//READ from ELF if we are not dealing with something on the stack

		loadsize = PAGE_SIZE;

		(void) fileBegin;
		//if the offset we end up with is
		if (uio_offset > fileEnd) {
			uio_offset = fileEnd - PAGE_SIZE; //needs to be revised
		}

		else if ((uio_offset < fileEnd)) {

			//reading a page from this point will go off of the segment
			if (uio_offset + PAGE_SIZE > fileEnd) {
				loadsize = fileEnd - uio_offset;
			}
		}

		KASSERT(loadsize <= PAGE_SIZE);
		uio_kinit(&iov, &u, (void *) PADDR_TO_KVADDR(paddr), loadsize,
				uio_offset, UIO_READ);
		result = VOP_READ(curproc->p_elf->v, &u);
		if (result) {
			DEBUG(DB_A3, "VOP_READ ERROR!!\n");
			return NULL;
		}

		KASSERT(u.uio_iov->iov_len == 0);

		if (u.uio_resid != 0) {
			/* short read; problem with executable? */
			kprintf("loadPTE: ELF: short read on segment - file truncated?\n");
			kprintf("uio_offset=%d, uio_resid is %d\n", (int) uio_offset,
					u.uio_resid);
			return NULL;
		}
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
	(void) pgTable;
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
/*
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
 */
