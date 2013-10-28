void sys__exit(int exitcode);
int sys_write(int fd, const void *buf, size_t size, int32_t *retval);
int sys_open(const char *filename, int flags, int mode, int32_t *retval);
int sys_close(int fd);
int sys_read(int fd, void* buf, size_t nbytes, int32_t *retval);
