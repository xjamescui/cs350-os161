#include <types.h>
#include <pt.h>

#define FILESIZE = 9 * 1024 * 1024;

//short for swap file entry
//don't forget to declare the sfe array volatile
struct sfe {
	struct addrspace * as;
	vaddr_t vaddr;
};

int initSF(void);

int copyToSwap(struct pte * entry);

int copyFromSwap(struct pte * entry);
