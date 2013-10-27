#include <types.h>
#include <synch.h>
#include <vnode.h>

//Value for these constants are subject to Change
const int MAX_LEN_FILENAME = 50;
const int MAX_OPEN_COUNT = 0;
const int MAX_FD_COUNT = 4; //for initializing the file descriptor table

struct file_desc {

	char f_name[MAX_LEN_FILENAME];
	off_t f_offset;
	int f_opencount;
	int f_refcount;
	struct lock* f_lock;
	struct vnode* f_vn;

};
