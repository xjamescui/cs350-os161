#include <types.h>
#include <lib.h>
#include <pt.h>
#include <proc.h>
#include <addrspace.h>
#include <current.h>
#include <mips/vm.h>
#include <kern/errno.h>
#include <coremap.h>

/**
 * called in as_define_region, after page numbers needed are made known
 */
void initPT(struct pt * pgTable) {

	int dbflags = DB_A3;

	KASSERT(pgTable == NULL);

	//find out how many pages the proc needs
	int numTextPages = curproc_getas()->as_npages_text;
	int numDataPages = curproc_getas()->as_npages_data;

	DEBUG(DB_A3, "numTextPages is %d\n", numTextPages);

	KASSERT(numTextPages > 0);
	KASSERT(numDataPages > 0);

	const int numStackPages = 12;
	(void) numStackPages;

	//create a pt

	//should we use kmalloc?
	pgTable->text = kmalloc(numTextPages * sizeof(struct pte));
	pgTable->data = kmalloc(numDataPages * sizeof(struct pte));



	//should we have one for the stack? what's going on here?
	//pageTable->text = (struct pte *) kmalloc (12 * sizeof(struct pte));

	//for every page that it needs, create a pte and init appropriate fields
//	for (int a = 0; a < numTextPages; a++) {
//		//init the ptes here
//	}
//
//	for (int a = 0; a < numDataPages; a++) {
//		//init the ptes here
//	}
}

paddr_t getPTE(struct pt* pgTable, vaddr_t addr) {

	KASSERT(pgTable != NULL);

	int dbflags = DB_A3;

	DEBUG(DB_A3, "we are in getPTE with vaddr =%x\n", addr);
	KASSERT(curproc_getas() != NULL);

	//text segment
	vaddr_t textBegin = curproc_getas()->as_vbase_text;
	vaddr_t textEnd = textBegin + curproc_getas()->as_npages_text * PAGE_SIZE;

	DEBUG(DB_A3, "textBegin =%x\n", textBegin);
	DEBUG(DB_A3, "textEnd = %x\n", textEnd);
	KASSERT(textEnd > textBegin);

	//data segment
	vaddr_t dataBegin = curproc_getas()->as_vbase_data;
	vaddr_t dataEnd = dataBegin + curproc_getas()->as_npages_data * PAGE_SIZE;

	DEBUG(DB_A3, "dataBegin =%x\n", dataBegin);
	DEBUG(DB_A3, "dataEnd = %x\n", dataEnd);

	KASSERT(dataEnd > dataBegin);

	int vpn;

	//not in either segments
	if (!((textBegin <= addr && addr <= textEnd)
			|| (dataBegin <= addr && addr <= dataEnd))) {
		//kill process
		DEBUG(DB_A3, "getPTE: addr is not in any segment\n");
		return EFAULT;
	}

	//in text segment
	else if (textBegin <= addr && addr <= textEnd) {
		vpn = ((addr - textBegin) & PAGE_FRAME) / PAGE_SIZE;

		if (pgTable->text[vpn]->valid) {
			//add mapping to tlb, evict victim if necessary
			//reexecute instruction
			DEBUG(DB_A3, "getPTE: addr belongs to text segment\n");
			return pgTable->text[vpn]->paddr;
		} else {
			//page fault
			DEBUG(DB_A3, "page fault in text segment with vaddr =%x\n", addr);
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
			DEBUG(DB_A3, "tlb miss: pte found in data segment\n");
			return pgTable->data[vpn]->paddr;
		} else {
			DEBUG(DB_A3, "page fault in text segment with vaddr =%x\n", addr);
			return loadPTE(pgTable, addr, 0, dataBegin);
		}
	}

	return EFAULT;
}

paddr_t loadPTE(struct pt * pgTable, vaddr_t vaddr, bool inText,
		vaddr_t segBegin) {

	KASSERT(pgTable != NULL);

	//we only need one page
	paddr_t paddr = cm_alloc_pages(1);

	//load page based on swapfile or ELF file
	if (elf_to_ram(curproc->p_elf, vaddr, paddr)) {
		return EFAULT;
	}

	//update page table entry: copy paddr over to page table
	int vpn = ((vaddr - segBegin) & PAGE_FRAME) / PAGE_SIZE;

	if (inText) {
		pgTable->text[vpn]->paddr = paddr;
	} else {
		pgTable->data[vpn]->paddr = paddr;
	}

	//something has gone wrong!
	return EFAULT;
}
/**
 * load a PAGE_SIZE of data from ELF to RAM at paddr
 * returns: success (0) or error (1)
 */
int elf_to_ram(struct vnode* v, vaddr_t vaddr, paddr_t paddr) {

	(void) v;
	(void) vaddr;
	(void) paddr;

	int dbflags = DB_A3;
	DEBUG(DB_A3, "paddr is %x\n", paddr);
	DEBUG(DB_A3, "vaddr is %x\n", vaddr);
//	struct iovec iov;
//	struct uio u;
//
//	iov.iov_base = (userptr_t) vaddr;
//	iov.iov_len = PAGE_SIZE;		 // length of the memory space
//	u.uio_iov = &iov;
//	u.uio_iovcnt = 1;
//	u.uio_resid = PAGE_SIZE;          // amount to read from the file
//	u.uio_offset = offset;
//	u.uio_segflg = is_executable ? UIO_USERISPACE : UIO_USERSPACE;
//	u.uio_rw = UIO_READ;
//	u.uio_space = as;

	return 0;
}
