#include <types.h>
#include <swapfile.h>
#include <addrspace.h>
#include <kern/iovec.h>
#include <synch.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <vfs.h>
#include <vnode.h>
#include <addrspace.h>
#include <uio.h>
#include <proc.h>
#include <lib.h>

int initSF() {
 //Create/open swap file
 char *name = kstrdup("SWAPFILE");
 vfs_open(name, O_RDWR | O_CREAT | O_TRUNC, 0, &swapv);

 //Calculate number of allowable entries
 swap_entries = FILESIZE/PAGE_SIZE; 

 sfeArray = kmalloc(swap_entries * sizeof(struct sfe*));

 for (int i = 0; i < swap_entries; i++) {
  sfeArray[i] = NULL;
 }

 swap_lock = lock_create("swap_lock");

	return 0;
}

int copyToSwap(struct page * entry) {
	//copies from pt to swapfile
 KASSERT(entry != NULL); 
 lock_acquire(swap_lock);
 struct addrspace *as = entry->as;
 int entry_index = -1;
 
 //Check for existing entry 
 for (int i = 0; i < swap_entries; i++) {
   if (sfeArray[i] != NULL) { //Non-empty entry
     struct sfe *temp = sfeArray[i]; //?
     if (temp->as == as && temp->vaddr == entry->vaddr) {
       entry_index = i;
       break;
     }
   }
 }

 //If no existing entry, find an empty entry
 if (entry_index == -1) {
   for (int i = 0; i < swap_entries; i++) {
     if (sfeArray[i] == NULL) {
       //The entry_index will be used to calc. offset in the swap file
       entry_index = i;
       break;
     }
   }
 }

 //No free space found. Exceeded swap file limit.
 if (entry_index == -1) {
  lock_release(swap_lock);
  return -1;
 }
 //Initialize stuff to write to file
 struct iovec iov;
 struct uio u;
 int result;

 iov.iov_kbase = (void *)PADDR_TO_KVADDR(entry->paddr);
 iov.iov_len = PAGE_SIZE;
 u.uio_iov = &iov;
 u.uio_iovcnt = 1;
 u.uio_resid = PAGE_SIZE;
 u.uio_offset = (off_t)(entry_index * PAGE_SIZE); 
 u.uio_segflg = UIO_SYSSPACE; 
 u.uio_rw = UIO_WRITE;
 u.uio_space = NULL; 

 result = VOP_WRITE(swapv, &u);
 if (result) {
  lock_release(swap_lock);
  return -1;
 }
 KASSERT(u.uio_resid == 0);

 //Update sfeArray
 //If empty entry, intialize before store
 struct sfe **entry_to_replace = &sfeArray[entry_index];//?
 if (*entry_to_replace == NULL) {
   *entry_to_replace = kmalloc(sizeof(struct sfe));

   if (*entry_to_replace == NULL) {
     lock_release(swap_lock);
     return -1;
   }
 }

 KASSERT(sfeArray[entry_index] != NULL); 

 //Either way, will store in new entry or overwrite old entry
 (*entry_to_replace)->as = as;
 (*entry_to_replace)->vaddr = entry->vaddr;
 
 lock_release(swap_lock);
	(void)entry;
	return 0;
}

int copyFromSwap(struct page * entry) {
	//copies from swap file to pt
lock_acquire(swap_lock);
 struct addrspace *as = entry->as;
 int entry_index = -1;
 
 //Find entry 
 for (int i = 0; i < swap_entries; i++) {
   if (sfeArray[i] != NULL) { //Non-empty entry
     struct sfe *temp = sfeArray[i];
     if (temp->as == as && temp->vaddr == entry->vaddr) {
       entry_index = i;
       break;
     }
   }
 }

 if (entry_index != -1) {
   lock_release(swap_lock);
   return 0;
 }

 //No entry found. Bad!
 if (entry_index == -1) {
  lock_release(swap_lock);
  return -1;
 }


 //Initialize stuff to read from file
 struct iovec iov;
 struct uio u;
 int result;

 iov.iov_kbase = (void *)PADDR_TO_KVADDR(entry->paddr);
 iov.iov_len = PAGE_SIZE;
 u.uio_iov = &iov;
 u.uio_iovcnt = 1;
 u.uio_resid = PAGE_SIZE;
 u.uio_offset = entry_index * PAGE_SIZE; //In bits/bytes?
 u.uio_segflg = UIO_SYSSPACE; //not sure
 u.uio_rw = UIO_READ;
 u.uio_space = NULL; 

 result = VOP_READ(swapv, &u);
 if (result) {
  lock_release(swap_lock);
  return -1;
 }
 KASSERT(u.uio_resid == 0);
 
 //Once entry is read, swap read is done

	(void)entry;
 lock_release(swap_lock);
	return 0;
}
