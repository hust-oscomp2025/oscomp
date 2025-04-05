#include <kernel/mm/kmalloc.h>
#include <kernel/mm/uaccess.h>
#include <kernel/sched/process.h>
#include <kernel/util/print.h>
#include <kernel/syscall/syscall.h>
#include <kernel/types.h>
#include <kernel/util/string.h>
#include <kernel/vfs.h>
#include <syscall.h>

/* Debug flag to enable syscall tracing */
#define SYSCALL_DEBUG 0

int64 sys_close(int32 fd) {
	/* Implementation here */
	return do_close(fd);
}





int64 sys_lseek(int32 fd, off_t offset, int32 whence) {
	/* Implementation here */
	return do_lseek(fd, offset, whence);
}

int64 sys_mount(const char* source, const char* target, const char* fstype, uint64 flags, const void* data) {
	/* Implementation here */
	return do_mount(source, target, fstype, flags, data);
}



int64 sys_getpid(void) {
	/* Implementation here */
	return current_task()->pid;
}





/* Complete syscall table using function pointers directly */
typedef int64 (*syscall_fn_t)(int64, int64, int64, int64, int64, int64);

static struct syscall_entry syscall_table[] = {
    /* File operations */
    [SYS_openat] = {(syscall_fn_t)sys_openat, "open", 3},
    [SYS_close] = {(syscall_fn_t)sys_close, "close", 1},
    [SYS_read] = {(syscall_fn_t)sys_read, "read", 3},
    [SYS_write] = {(syscall_fn_t)sys_write, "write", 3},
    [SYS_lseek] = {(syscall_fn_t)sys_lseek, "lseek", 3},
    [SYS_mount] = {(syscall_fn_t)sys_mount, "mount", 5},
    [SYS_getdents64] = {(syscall_fn_t)sys_getdents64, "getdents", 3},

    /* Process operations */
    [SYS_exit] = {(syscall_fn_t)sys_exit, "exit", 1},
    [SYS_getpid] = {(syscall_fn_t)sys_getpid, "getpid", 0},
    [SYS_getppid] = {NULL, "getppid", 0}, // Not implemented yet
    [SYS_clone] = {(syscall_fn_t)sys_clone, "clone", 5},

    /* Memory operations */
    [SYS_mmap] = {(syscall_fn_t)sys_mmap, "mmap", 6},
    [SYS_brk] = {NULL, "brk", 1}, // Not implemented yet

    /* Time operations */
    //[SYS_time] = {(syscall_fn_t)sys_time, "time", 1},

    /* Add more syscalls as needed */
};

#define SYSCALL_TABLE_SIZE (sizeof(syscall_table) / sizeof(syscall_table[0]))

/**
 * The main syscall dispatcher
 */
long do_syscall(long syscall_num, long a0, long a1, long a2, long a3, long a4, long a5) {

	/* Validate syscall number */
	if (syscall_num < 0 || syscall_num >= SYSCALL_TABLE_SIZE || !syscall_table[syscall_num].func) {
		kprintf("Invalid syscall: %ld\n", syscall_num);
		return -ENOSYS;
	}

	struct syscall_entry* entry = &syscall_table[syscall_num];

#if SYSCALL_DEBUG
	/* Debug output before syscall */
	kprintf("SYSCALL: %s(%ld, %ld, ...)\n", entry->name, a0, a1);
#endif

	/* Execute syscall through the function pointer with type casting */
	int64 ret = entry->func(a0, a1, a2, a3, a4, a5);

#if SYSCALL_DEBUG
	/* Debug output after syscall */
	kprintf("SYSCALL: %s returned %ld\n", entry->name, ret);
#endif

	return ret;
}

/* Add the rest of your syscall implementations here */