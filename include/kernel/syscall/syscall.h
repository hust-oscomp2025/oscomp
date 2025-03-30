#ifndef _SYSCALL_H_
#define _SYSCALL_H_
#include <kernel/syscall/syscall_fs.h>
#include <kernel/syscall/syscall_ids.h>
#include <kernel/types.h>

/**
 * System call entry in dispatch table
 */
struct syscall_entry {
    syscall_func_t func;  // The wrapper function pointer
    const char *name;     // Name for debugging
    uint8_t num_args;     // Number of arguments (for validation)
};



extern struct syscall_function sys_table[];

/**
 * System call wrapper type - all syscall wrappers must match this signature
 * They receive raw int64 arguments and return int64
 */
typedef int64 (*syscall_func_t)(int64, int64, int64, int64, int64, int64);


int64 do_syscall(int64 a0, int64 a1, int64 a2, int64 a3, int64 a4, int64 a5, int64 a6, int64 a7);
ssize_t sys_user_yield();
#endif
