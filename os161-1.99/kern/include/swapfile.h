#include <types.h>
#include <pt.h>

#define FILESIZE 9 * 1024 * 1024

//short for swap file entry
//don't forget to declare the sfe array volatile
struct sfe {
	struct addrspace * as;
	vaddr_t vaddr;
};

struct sfe ** sfeArray;
struct vnode *swapv;
volatile int swap_entries;
//Need a swap lock here
struct lock *swap_lock;

int initSF(void);

int copyToSwap(struct pte * entry);

int copyFromSwap(struct pte * entry);
