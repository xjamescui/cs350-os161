#include <types.h>

struct lock;
struct vnode;

//Value for these constants are subject to Change
#define MAX_LEN_FILENAME  50

struct file_desc {

	char* f_name; //should set MAX_LEN_FILENAME on it later
	off_t f_offset;
	int f_opencount;
	int f_refcount;
	struct lock* f_lock;
	struct vnode* f_vn;

};
