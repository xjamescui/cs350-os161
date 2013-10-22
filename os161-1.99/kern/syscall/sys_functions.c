#include <types.h>
#include <copyinout.h>
#include <syscall.h>
#include <lib.h>

int 
sys_write(int filehandle, const void *buf, size_t size) {
  (void)filehandle;
  (void)size;
  int wroteBytes = kprintf(buf);
  return wroteBytes;
}

//copyout
