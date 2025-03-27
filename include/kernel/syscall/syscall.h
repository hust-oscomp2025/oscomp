#ifndef _SYSCALL_H_
#define _SYSCALL_H_
#include <kernel/syscall/syscall_fs.h>
#include <kernel/types.h>

typedef struct syscall_function {
	uint64 func;
	const char *name;
} syscall_function;

extern struct syscall_function sys_table[];


int64 do_syscall(int64 a0, int64 a1, int64 a2, int64 a3, int64 a4, int64 a5, int64 a6, int64 a7);
ssize_t sys_user_yield();
#endif
