#ifndef _UACCESS_H
#define _UACCESS_H

#include <kernel/types.h>
#include <kernel/mm/mm_struct.h>
#include <kernel/sched/sched.h>

char *user_to_kernel_str(const char *user_ptr);
unsigned long copy_from_user(void *to, const void __user *from, unsigned long n);
unsigned long copy_to_user(void __user *to, const void *from, unsigned long n);
long strlen_user(const char __user *str);
long strncpy_from_user(char *dst, const char __user *src, long count);
unsigned long clear_user(void __user *to, unsigned long n);
int access_ok(const void __user *addr, unsigned long size);

// 更简单的单值访问宏
#define get_user(x, ptr) ({ \
    unsigned long __ret; \
    __ret = copy_from_user(&(x), (ptr), sizeof(*(ptr))); \
    __ret ? -EFAULT : 0; \
})

#define put_user(x, ptr) ({ \
    unsigned long __ret; \
    __ret = copy_to_user((ptr), &(x), sizeof(*(ptr))); \
    __ret ? -EFAULT : 0; \
})










#endif /* _UACCESS_H */