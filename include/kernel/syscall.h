/*
 * define the syscall numbers of PKE OS kernel.
 */
#ifndef _SYSCALL_H_
#define _SYSCALL_H_
#include <kernel/types.h>

typedef struct syscall_function {
	void *func;
	const char *name;
} syscall_function;

extern struct syscall_function sys_table[];


long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7);
ssize_t sys_user_yield();
#endif
