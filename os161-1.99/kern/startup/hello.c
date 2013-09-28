#include<types.h>
#include<lib.h>
#include<test.h>

void hello(){
	kprintf("Hello World\n");

	// dbflags = DB_CATMOUSE | DB_THREADS;
	// DEBUG(DB_CATMOUSE, "Hello World\n");
	// DEBUG(DB_THREADS, "DB_THREADS: HELLO WORLD\n");
	// DEBUG(DB_NET, "DB_NET debug message\n");
	// DEBUG(DB_VM, "DB_VM debug message\n");
}
