#include <kernel/mm/kmalloc.h>
#include <kernel/mm/uaccess.h>
#include <kernel/sched/process.h>
#include <kernel/sprint.h>
#include <kernel/syscall/syscall.h>
#include <kernel/types.h>
#include <kernel/util/string.h>
#include <kernel/vfs.h>

/* Debug flag to enable syscall tracing */
#define SYSCALL_DEBUG 0

int64 sys_close(int32 fd) {
	/* Implementation here */
	return do_close(fd);
}

int64 sys_read(int32 fd, void* buf, size_t count) {
	void* kbuf = kmalloc(count);
	if (!kbuf) return -ENOMEM;
	int ret = do_read(fd, kbuf, count);
	if (ret < 0) {
		kfree(kbuf);
		return ret;
	}
	copy_to_user((void __user*)buf, kbuf, ret);
	/* Implementation here */
	return ret;
}

int64 sys_write(int32 fd, const void* buf, size_t count) {
	void* kbuf = kmalloc(count);
	if (!kbuf) return -ENOMEM;
	if (copy_from_user(kbuf, buf, count)) {
		kfree(kbuf);
		return -EFAULT;
	}
	int ret = do_write(fd, kbuf, count);
	if (ret < 0) {
		kfree(kbuf);
		return ret;
	}
	kfree(kbuf);
	/* Implementation here */
	return ret;
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





int64 sys_time(time_t __user* tloc) {
    time_t ktime;
    int64 ret = do_time(&ktime);  // 假设 do_time 填充内核时间到 ktime

    if (ret < 0) {
        return ret;  // 错误直接返回
    }

    // 如果用户传递的 tloc 非空，需要将 ktime 写回用户空间
    if (tloc) {
        if (copy_to_user(tloc, &ktime, sizeof(time_t)) != 0) {
            return -EFAULT;  // 用户指针无效或不可写
        }
    }

    return ktime;  // 返回时间值（兼容 tloc==NULL 的场景）
}

/* Complete syscall table using function pointers directly */
typedef int64 (*syscall_fn_t)(int64, int64, int64, int64, int64, int64);

static struct syscall_entry syscall_table[] = {
    /* File operations */
    [__NR_openat] = {(syscall_fn_t)sys_openat, "open", 3},
    [__NR_close] = {(syscall_fn_t)sys_close, "close", 1},
    [__NR_read] = {(syscall_fn_t)sys_read, "read", 3},
    [__NR_write] = {(syscall_fn_t)sys_write, "write", 3},
    [__NR_lseek] = {(syscall_fn_t)sys_lseek, "lseek", 3},
    [__NR_mount] = {(syscall_fn_t)sys_mount, "mount", 5},
    [__NR_getdents64] = {(syscall_fn_t)sys_getdents64, "getdents", 3},

    /* Process operations */
    [__NR_exit] = {(syscall_fn_t)sys_exit, "exit", 1},
    [__NR_getpid] = {(syscall_fn_t)sys_getpid, "getpid", 0},
    [__NR_getppid] = {NULL, "getppid", 0}, // Not implemented yet
    [__NR_clone] = {(syscall_fn_t)sys_clone, "clone", 5},

    /* Memory operations */
    [__NR_mmap] = {(syscall_fn_t)sys_mmap, "mmap", 6},
    [__NR_brk] = {NULL, "brk", 1}, // Not implemented yet

    /* Time operations */
    //[__NR_time] = {(syscall_fn_t)sys_time, "time", 1},

    /* Add more syscalls as needed */
};

#define SYSCALL_TABLE_SIZE (sizeof(syscall_table) / sizeof(syscall_table[0]))

/**
 * The main syscall dispatcher
 */
int64 do_syscall(int64 syscall_num, int64 a0, int64 a1, int64 a2, int64 a3, int64 a4, int64 a5) {

	/* Validate syscall number */
	if (syscall_num < 0 || syscall_num >= SYSCALL_TABLE_SIZE || !syscall_table[syscall_num].func) {
		sprint("Invalid syscall: %ld\n", syscall_num);
		return -ENOSYS;
	}

	struct syscall_entry* entry = &syscall_table[syscall_num];

#if SYSCALL_DEBUG
	/* Debug output before syscall */
	sprint("SYSCALL: %s(%ld, %ld, ...)\n", entry->name, a0, a1);
#endif

	/* Execute syscall through the function pointer with type casting */
	int64 ret = entry->func(a0, a1, a2, a3, a4, a5);

#if SYSCALL_DEBUG
	/* Debug output after syscall */
	sprint("SYSCALL: %s returned %ld\n", entry->name, ret);
#endif

	return ret;
}

/* Add the rest of your syscall implementations here */