#ifndef _LINUX_MM_H
#define _LINUX_MM_H

#include <kernel/mm/pagetable.h>
#include <kernel/riscv.h>
#include <kernel/types.h>
#include <kernel/process.h>
#include <kernel/mm/page.h>

/**
 * 虚拟内存区域类型
 */
enum vma_type {
  VMA_ANONYMOUS = 0, // 匿名映射（如堆）
  VMA_PRIVATE,       // 私有映射
  VMA_SHARED,        // 共享映射
  VMA_FILE,          // 文件映射
  VMA_STACK,         // 栈区域
  VMA_HEAP,          // 堆区域
  VMA_TEXT,          // 代码段
  VMA_DATA,          // 数据段
  VMA_BSS,           // BSS段
  VMA_VDSO           // 虚拟动态共享对象
};


/* 页标志位定义 */
#define VM_READ       (1UL << 0)  /* 可读 */
#define VM_WRITE      (1UL << 1)  /* 可写 */
#define VM_EXEC       (1UL << 2)  /* 可执行 */
#define VM_SHARED     (1UL << 3)  /* 共享映射 */
#define VM_PRIVATE    (1UL << 4)  /* 私有映射（写时复制） */
#define VM_MAYREAD    (1UL << 5)  /* 可设置为可读 */
#define VM_MAYWRITE   (1UL << 6)  /* 可设置为可写 */
#define VM_MAYEXEC    (1UL << 7)  /* 可设置为可执行 */
#define VM_MAYSHARE   (1UL << 8)  /* 可设置为共享 */
#define VM_GROWSDOWN  (1UL << 9)  /* 向下增长(栈) */
#define VM_GROWSUP    (1UL << 10) /* 向上增长(堆) */
#define VM_USER       (1UL << 11) /* 用户空间 */
#define VM_DONTCOPY   (1UL << 12) /* fork时不复制 */
#define VM_DONTEXPAND (1UL << 13) /* 不允许扩展 */
#define VM_LOCKED     (1UL << 14) /* 页面锁定，不允许换出 */
#define VM_IO         (1UL << 15) /* 映射到I/O地址空间 */

/* 权限组合掩码 */
#define VM_ACCESS_FLAGS (VM_READ | VM_WRITE | VM_EXEC)
#define VM_MAYACCESS    (VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC | VM_MAYSHARE)

/* mmap函数的标志位 */
#define MAP_SHARED     0x01        /* 共享映射 */
#define MAP_PRIVATE    0x02        /* 私有映射(写时复制) */
#define MAP_FIXED      0x10        /* 精确解释addr参数 */
#define MAP_ANONYMOUS  0x20        /* 匿名映射，不对应文件 */
#define MAP_GROWSDOWN  0x0100      /* 栈向下增长 */
#define MAP_DENYWRITE  0x0800      /* 不允许对内存区写入 */
#define MAP_EXECUTABLE 0x1000      /* 允许执行 */
#define MAP_LOCKED     0x2000      /* 锁定页面 */
#define MAP_POPULATE   0x8000      /* 预先填充页表 */

/* PROT_* 定义 */
#define PROT_NONE      0x0         /* 页不可访问 */
#define PROT_READ      0x1         /* 页可读 */
#define PROT_WRITE     0x2         /* 页可写 */
#define PROT_EXEC      0x4         /* 页可执行 */



#endif /* _LINUX_MM_H */