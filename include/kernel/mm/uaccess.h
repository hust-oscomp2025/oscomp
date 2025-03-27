#ifndef _UACCESS_H
#define _UACCESS_H

#include <kernel/types.h>
#include <kernel/mm/mm_struct.h>
#include <kernel/sched/sched.h>

char *user_to_kernel_str(const char *user_ptr);
uint64 copy_from_user(void *to, const void __user *from, uint64 n);
uint64 copy_to_user(void __user *to, const void *from, uint64 n);
int64 strlen_user(const char __user *str);
int64 strncpy_from_user(char *dst, const char __user *src, int64 count);
uint64 clear_user(void __user *to, uint64 n);
int32 access_ok(const void __user *addr, uint64 size);

// 更简单的单值访问宏
#define get_user(x, ptr) ({ \
    uint64 __ret; \
    __ret = copy_from_user(&(x), (ptr), sizeof(*(ptr))); \
    __ret ? -EFAULT : 0; \
})

#define put_user(x, ptr) ({ \
    uint64 __ret; \
    __ret = copy_to_user((ptr), &(x), sizeof(*(ptr))); \
    __ret ? -EFAULT : 0; \
})










#endif /* _UACCESS_H */