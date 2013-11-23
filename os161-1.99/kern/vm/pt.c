#include <pt.h>

void initPT(void) {
	//find out how many pages the proc needs
	
	//create a pt

	//for every page that it needs, create a pte and init appropriate fields
}

int getPTE(vaddr_t addr) {
	
	//boundary checking?

	//for every pte, check if page number * pagesize <= addr < page number +1 * pagesize
	
	(void) addr;
	return 0; //change this later
}
