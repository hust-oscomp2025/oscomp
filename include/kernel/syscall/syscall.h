#ifndef _SYSCALL_H_
#define _SYSCALL_H_
#include <kernel/syscall/syscall_fs.h>
#include <kernel/syscall/syscall_ids.h>
#include <kernel/types.h>

struct syscall_function {
	void* func;
	const char *name;
};

extern struct syscall_function sys_table[];


int64 do_syscall(int64 a0, int64 a1, int64 a2, int64 a3, int64 a4, int64 a5, int64 a6, int64 a7);
ssize_t sys_user_yield();
#endif
