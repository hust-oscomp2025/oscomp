#ifndef _SYSCALL_HEAP_H
#define _SYSCALL_HEAP_H

#include <kernel/types.h>

// 设置/获取程序数据段结束地址
uint64 sys_brk(uint64 addr);

// 创建内存映射
uint64 sys_mmap(uint64 addr, size_t length, int prot, int flags,
                       int fd, off_t offset);

// 取消内存映射
int sys_munmap(uint64 addr, size_t length);

// 修改内存区域保护属性
int sys_mprotect(uint64 addr, size_t len, int prot);

// 重新映射虚拟内存地址
uint64 sys_mremap(uint64 old_address, size_t old_size, 
                         size_t new_size, int flags, ... /* __user uint64 new_address */);

// 提供关于内存使用的建议
int sys_madvise(uint64 addr, size_t length, int advice);




#endif