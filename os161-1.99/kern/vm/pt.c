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
#include <uw-vmstats.h>

#define DUMBVM_STACKPAGES    12
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

/**
 * Initializes/Sets up the page table, after it has been alloc'ed
 */
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
		pgTable->text[a]->paddr = 0;
	}

	for (int a = 0; a < numDataPages; a++) {
		pgTable->data[a] = kmalloc(sizeof(struct pte));
		pgTable->data[a]->valid = false;
		pgTable->data[a]->readOnly = false;
		pgTable->data[a]->paddr = 0;

	}

	for (int a = 0; a < DUMBVM_STACKPAGES; a++) {
		pgTable->stack[a] = kmalloc(sizeof(struct pte));
		pgTable->stack[a]->valid = false;
		pgTable->stack[a]->readOnly = false;
		pgTable->stack[a]->paddr = 0;
	}

}

/**
 * TLB Miss and we are now trying to find what we need in the page table
 */
struct pte * getPTE(struct pt* pgTable, vaddr_t addr, int* inText) {

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
		return NULL;
	}

	//in text segment
	else if (textBegin <= addr && addr <= textEnd) {
		*inText = 1;
		vpn = ((addr - textBegin) & PAGE_FRAME) / PAGE_SIZE;
		entry = pgTable->text[vpn];
		if (entry->valid) {
			vmstats_inc(VMSTAT_TLB_RELOAD);
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
			vmstats_inc(VMSTAT_TLB_RELOAD);
			return entry;
		} else {
			//page fault
			return loadPTE(pgTable, addr, DATA_SEG, dataBegin, dataEnd);
		}
	}
	//in stack segment
	else {
		vpn = ((addr - stackBase) & PAGE_FRAME) / PAGE_SIZE;
		entry = pgTable->stack[vpn];

		if (entry->valid) {
			vmstats_inc(VMSTAT_TLB_RELOAD);
			return entry;
		} else {
			//page fault
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

//	kprintf("Trying to load vaddr %x in segment %d with range %x to %x\n",
//			faultaddr, segmentNum, segBegin, segEnd);

	if (faultaddr > MIPS_KSEG0) {
		panic("We are given a kernel vaddr!!!\n");
	}

	//which page number to search for
	int vpn = ((faultaddr - segBegin) & PAGE_FRAME) / PAGE_SIZE;

	uint32_t textFileBegin = curproc->p_elf->elf_text_offset;
	uint32_t dataFileBegin = curproc->p_elf->elf_data_offset;

	int fileSize = (segmentNum == TEXT_SEG) ?
	curproc->p_elf->elf_text_filesz : curproc->p_elf->elf_data_filesz;

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

	//getting a physical page to write to
	paddr_t paddr;
	long allocPageResult = getppages(1);
	if (allocPageResult > 0) {
		paddr = (paddr_t) allocPageResult;

		//ALERT: there may be a bug here
	} else if (allocPageResult == -1) {
		//we ran out of physical memory
		//need to find a victim in coremap to evict and
		//invalidate the corresponding pte in page table of the process
		//owning the page

		kprintf("error in loadPTE\n");
		return NULL;
		//NOTE: the swapping
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
	case STACK_SEG:
		uio_offset = 0;
		break;
	default:
		break;
	}

	//setup our UIO
	iov.iov_kbase = (void *) PADDR_TO_KVADDR(paddr);
	iov.iov_len = PAGE_SIZE;
	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_offset = uio_offset;
	u.uio_rw = UIO_READ;
	u.uio_space = NULL;
	u.uio_segflg = UIO_SYSSPACE;

	if (segmentNum == STACK_SEG) {
		u.uio_resid = PAGE_SIZE;
		if (uiomovezeros(PAGE_SIZE, &u)) {
			panic("error while zeroing for a stack page\n");
		}
	} else {
		//READ from ELF if we are not dealing with data or text segments (ELF does not
		//have a stack)

		//set up flags to see if zeroing is needed
		bool letsZero = PAGE_SIZE * (vpn + 1) >= fileSize; //there are unused spaces in the frame so we need to zero these spaces
		bool zeroWholePage = PAGE_SIZE * (vpn) >= fileSize; //offset is on the bound or going off, don't read anything
		size_t zeroAmt = 0;

		//setup our uio_resid
		if (letsZero) {

			if (zeroWholePage) {
				u.uio_resid = 0; //don't read anything but zero the page later
			} else {
				u.uio_resid = fileSize - vpn * PAGE_SIZE;
			}
		} else {
			//read a whole page
			u.uio_resid = PAGE_SIZE;
		}

		//stat tracking: reading from the ELF file
		vmstats_inc(VMSTAT_ELF_FILE_READ);

		//Reading...
		result = VOP_READ(curproc->p_elf->v, &u);
		if (result) {
			panic("error on VOP_READ from elf\n");
			return NULL;
		}

		if (u.uio_resid != 0) {
			/* short read; problem with executable? */
			panic("loadPTE: ELF: short read on segment - file truncated?\n");
			return NULL;
		}

		//filling zeroes if needed
		if (letsZero) {
			if (zeroWholePage) {
				zeroAmt = PAGE_SIZE;
			} else {
				zeroAmt = (vpn + 1) * PAGE_SIZE - fileSize;
			}

			if (zeroAmt > 0) {
				u.uio_resid += zeroAmt;
				if (uiomovezeros(zeroAmt, &u)) {
					panic("uiomovezero error in loadPTE");
				}
			}

		}

	}

	//update info to page table
	if (segmentNum == TEXT_SEG) {
		//text-segment is readonly!
		pgTable->text[vpn]->vaddr = faultaddr;
		pgTable->text[vpn]->paddr = paddr;
		pgTable->text[vpn]->valid = 1;
		pgTable->text[vpn]->readOnly = 1;
		return pgTable->text[vpn];
	} else if (segmentNum == DATA_SEG) {
		pgTable->data[vpn]->vaddr = faultaddr;
		pgTable->data[vpn]->paddr = paddr;
		pgTable->data[vpn]->valid = 1;
		pgTable->data[vpn]->readOnly = 0;
		return pgTable->data[vpn];
	} else if (segmentNum == STACK_SEG) {
		pgTable->stack[vpn]->vaddr = faultaddr;
		pgTable->stack[vpn]->paddr = paddr;
		pgTable->stack[vpn]->valid = 1;
		pgTable->stack[vpn]->readOnly = 0;
		return pgTable->stack[vpn];
	} else {
		//something wrong!
		return NULL;
	}

	//something has gone wrong!
	return NULL;
}

int destroyPT(struct pt * pgTable) {
	for (unsigned int a = 0; a < pgTable->numTextPages; a++) {
		if (pgTable->text[a]->paddr != 0) {
			free_page(pgTable->text[a]->paddr);
		}
	}

	for (unsigned int a = 0; a < pgTable->numDataPages; a++) {
		if (pgTable->data[a]->paddr != 0) {
			free_page(pgTable->data[a]->paddr);
		}
	}

	for (unsigned int a = 0; a < DUMBVM_STACKPAGES; a++) {
		if (pgTable->stack[a]->paddr != 0) {
			free_page(pgTable->stack[a]->paddr);
		}
	}

	return 0;
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
