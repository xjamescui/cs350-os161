#include <swapfile.h>
#include <addrspace.h>


int initSF() {
	//if swapfile file size > 9MB, panic
	return 0;
}

int copyToSwap(struct pte * entry) {
	//copies from pt to swapfile
	(void)entry;
	return 0;
}

int copyFromSwap(struct pte * entry) {
	//copies from swap file to pt
	(void)entry;
	return 0;
}
