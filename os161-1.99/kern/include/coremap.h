//alloc methods
paddr_t alloc_page(void);
paddr_t alloc_pages(int npages);
void free_page(vaddr_t addr);

#define FREE 0
#define FIXED 1
#define CLEAN 2
#define DIRTY 3

/* a physical page (frame) for coremap */
struct page {
	struct addrspace * as;

	//the vaddr the frame is mapped to
	volatile vaddr_t vaddr;
	volatile paddr_t paddr;
	volatile int vpn;

	/**
	 * n for n consecutive blocks allocated and this is the first block
 	 * -1 otherwise
	 **/
	volatile int blocksAllocated;

	/**
	 * 0 = free
	 * 1 = fixed
	 * 2 = clean
	 * 3 = dirty
	 **/
	volatile int state;
};


struct lock * coremapLock;
struct page * coremap;
