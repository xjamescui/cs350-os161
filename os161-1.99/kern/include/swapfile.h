#ifndef _SWAP_H_
#define _SWAP_H_

#include <types.h>
#include <pt.h>
#include <coremap.h>
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
volatile int swapinit;

int initSF(void);

int copyToSwap(struct page * entry);

int copyFromSwap(struct page * entry);
#endif
