#include <kernel/sched.h>
#include <kernel/vfs.h>
#include <kernel/syscall/syscall.h>
#include <kernel/mmu.h>
#include <kernel/util.h>


int64 sys_clone(uint64 flags, uint64 stack, uint64 ptid, uint64 tls, uint64 ctid) {
	/* Implementation here */
	return do_clone(flags, stack, ptid, tls, ctid);
}

int64 do_clone(uint64 flags, uint64 stack, uint64 ptid, uint64 tls, uint64 ctid){


	// TODO: clone
	return 0;
}
