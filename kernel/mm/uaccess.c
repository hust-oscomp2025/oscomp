#include <kernel/mm/mm_struct.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/process.h>
#include <kernel/types.h>


/**
 * copy_to_user - Copy a block of data into user space
 * @to: Destination address in user space
 * @from: Source address in kernel space
 * @n: Number of bytes to copy
 *
 * Returns number of bytes that could not be copied.
 * On success, this will be zero.
 */
unsigned long copy_to_user(void __user *to, const void *from, unsigned long n)
{
    struct task_struct *current = current_task();
    if (!current || !current->mm)
        return n;
        
    ssize_t ret = mm_copy_to_user(current->mm, (uint64)to, from, n);
    if (ret < 0)
        return n;
        
    return n - ret;
}

/**
 * strlen_user - Get string length in user space
 * @str: The string to measure
 *
 * Returns string length including null byte on success,
 * 0 on exception.
 */
long strlen_user(const char __user *str)
{
    long res = 0;
    char c;
    
    while (1) {
        if (copy_from_user(&c, str + res, 1) != 0)
            return 0;
            
        if (c == '\0')
            return res + 1; // 包含null字节
            
        res++;
        
        // 安全检查，避免无限循环
        if (res > 4096) // 合理的字符串长度上限
            return 0;
    }
}

/**
 * user_to_kernel_str - 将用户空间字符串复制到内核空间
 * @user_ptr: 用户空间字符串指针
 *
 * 分配内核内存并复制用户空间字符串
 * 调用者负责使用 kfree() 释放返回的内存
 *
 * 返回: 成功时返回内核空间字符串指针，失败返回 NULL
 */
char *user_to_kernel_str(const char *user_ptr) {
    char *kernel_ptr;
    int len;
    
    // 在此获取字符串长度和验证用户指针的有效性
    // 伪代码：验证用户空间指针和获取字符串长度
    len = strlen_user(user_ptr);
    if (len <= 0)
        return NULL;
    
    // 分配内核内存
    kernel_ptr = kmalloc(len + 1);
    if (!kernel_ptr)
        return NULL;
        
    // 复制字符串从用户空间到内核空间
    // 伪代码：实际实现应使用 copy_from_user 或等效函数
    if (copy_from_user(kernel_ptr, user_ptr, len + 1)) {
        kfree(kernel_ptr);
        return NULL;
    }
    
    kernel_ptr[len] = '\0'; // 确保字符串结束
    return kernel_ptr;
}


/**
 * copy_from_user - Copy a block of data from user space
 * @to: Destination address in kernel space
 * @from: Source address in user space
 * @n: Number of bytes to copy
 *
 * Returns number of bytes that could not be copied.
 * On success, this will be zero.
 */
unsigned long copy_from_user(void *to, const void __user *from, unsigned long n)
{
    struct task_struct *current = current_task();
    if (!current || !current->mm)
        return n;
        
    ssize_t ret = mm_copy_from_user(current->mm, (uint64)to, from, n);
    if (ret < 0)
        return n;
        
    return n - ret;
}


/**
 * strncpy_from_user - Copy a string from user space
 * @dst: Destination address in kernel space
 * @src: Source address in user space
 * @count: Maximum number of bytes to copy, including the trailing NUL
 *
 * Returns the length of the string (not including the trailing NUL) on success,
 * -EFAULT on access exceptions, or -ENAMETOOLONG if the string is too long.
 */
long strncpy_from_user(char *dst, const char __user *src, long count)
{
    long res = 0;
    
    if (count <= 0)
        return 0;
        
    while (res < count - 1) {
        if (copy_from_user(dst + res, src + res, 1) != 0)
            return -EFAULT;
            
        if (dst[res] == '\0')
            return res;
            
        res++;
    }
    
    dst[res] = '\0'; // 确保字符串结束
    
    // 检查源字符串是否真的结束
    char tmp;
    if (copy_from_user(&tmp, src + res, 1) != 0)
        return -EFAULT;
        
    if (tmp != '\0')
        return -ENAMETOOLONG; // 字符串太长
        
    return res; // 返回不包括结束符的长度
}


/**
 * clear_user - Zero a block of memory in user space
 * @to: Destination address in user space
 * @n: Number of bytes to zero
 *
 * Returns number of bytes that could not be cleared.
 * On success, this will be zero.
 */
unsigned long clear_user(void __user *to, unsigned long n)
{
    char zeros[64];
    unsigned long chunk, ret = 0;
    
    memset(zeros, 0, sizeof(zeros));
    
    while (n > 0) {
        chunk = (n > sizeof(zeros)) ? sizeof(zeros) : n;
        ret = copy_to_user(to, zeros, chunk);
        if (ret)
            break;
            
        to += chunk;
        n -= chunk;
    }
    
    return n; // 返回未能清零的字节数
}


/**
 * access_ok - 验证用户空间指针
 * @addr: 用户空间地址
 * @size: 访问大小
 *
 * 返回非零表示可以访问
 */
int access_ok(const void __user *addr, unsigned long size)
{
    struct task_struct *current = current_task();
    if (!current || !current->mm)
        return 0;
	return 0;
    // // 简单的边界检查，实际实现应该更完善
    // uint64 uaddr = (uint64)addr;
    // return (uaddr < TASK_SIZE) && (size <= TASK_SIZE - uaddr);
}

