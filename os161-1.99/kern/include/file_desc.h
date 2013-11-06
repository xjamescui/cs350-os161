#include <types.h>

struct lock;
struct vnode;

//Value for these constants are subject to Change
#define MAX_LEN_FILENAME  60

struct file_desc {

	char* f_name;
	off_t f_offset;
	struct lock* f_lock;
	struct vnode* f_vn;
	int f_flags;
	bool available;

};
