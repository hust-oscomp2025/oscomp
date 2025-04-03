#include <kernel/sched.h>
#include <kernel/vfs.h>
#include <kernel/syscall/syscall.h>
#include <kernel/mmu.h>
#include <kernel/util.h>




int64 sys_exit(int32 status) {
	/* Implementation here */
	return do_exit(status);
}

int64 do_exit(int32 status) {
	// TODO: Implement the exit logic
	return 0;
}