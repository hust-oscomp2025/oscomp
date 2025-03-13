#ifndef _LINUX_MM_H
#define _LINUX_MM_H

#include <kernel/mm/page.h>
#include <kernel/mm/pagetable.h>
#include <kernel/sched/process.h>
#include <kernel/riscv.h>
#include <kernel/types.h>

/* mmap函数的标志位 */
// mmap_flags
#define MAP_SHARED 0x01       /* 共享映射 */
#define MAP_PRIVATE 0x02      /* 私有映射(写时复制) */
#define MAP_FIXED 0x10        /* 精确解释addr参数 */
#define MAP_ANONYMOUS 0x20    /* 匿名映射，不对应文件 */
#define MAP_GROWSDOWN 0x0100  /* 栈向下增长 */
#define MAP_DENYWRITE 0x0800  /* 不允许对内存区写入 */
#define MAP_EXECUTABLE 0x1000 /* 允许执行 */
#define MAP_LOCKED 0x2000     /* 锁定页面 */
#define MAP_POPULATE 0x8000   /* 预先填充页表 */

/* PROT_* 定义 */
#define PROT_NONE 0x0  /* 页不可访问 */
#define PROT_READ 0x1  /* 页可读 */
#define PROT_WRITE 0x2 /* 页可写 */
#define PROT_EXEC 0x4  /* 页可执行 */

uint64 do_mmap(struct mm_struct *mm, uint64 addr, size_t length, int prot,
               uint64 flags, struct file *file, uint64 pgoff);
int do_unmap(struct mm_struct *mm, uint64 start, size_t len);
uint64 do_brk(struct mm_struct *mm, uint64 new_brk);
int do_protect(struct mm_struct *mm, __page_aligned uint64 start, size_t len, int prot);




#endif /* _LINUX_MM_H */