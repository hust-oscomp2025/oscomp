#ifndef _LINUX_MM_H
#define _LINUX_MM_H

#include <kernel/pagetable.h>
#include <kernel/riscv.h>
#include <kernel/types.h>

#define PTE_PFN_MASK (~(PAGE_SIZE - 1))

/* mmap.h */

/**
 * 虚拟内存区域标志位 - 可按位组合使用
 */
/* 基本权限标志 */
#define VM_READ (1UL << 0)  /* 可读 */
#define VM_WRITE (1UL << 1) /* 可写 */
#define VM_EXEC (1UL << 2)  /* 可执行 */

/* 共享属性标志 */
#define VM_SHARED (1UL << 3)  /* 共享映射 */
#define VM_PRIVATE (1UL << 4) /* 私有映射（写时复制） */

/* 可设置权限标志 */
#define VM_MAYREAD (1UL << 5)  /* 可设置为可读 */
#define VM_MAYWRITE (1UL << 6) /* 可设置为可写 */
#define VM_MAYEXEC (1UL << 7)  /* 可设置为可执行 */
#define VM_MAYSHARE (1UL << 8) /* 可设置为共享 */

/* 增长方向标志 */
#define VM_GROWSDOWN (1UL << 9) /* 向下增长(栈) */
#define VM_GROWSUP (1UL << 10)  /* 向上增长(堆) */

/* 特殊属性标志 */
#define VM_USER (1UL << 11)       /* 用户空间 */
#define VM_DONTCOPY (1UL << 12)   /* fork时不复制 */
#define VM_DONTEXPAND (1UL << 13) /* 不允许扩展 */
#define VM_LOCKED (1UL << 14)     /* 页面锁定，不允许换出 */
#define VM_IO (1UL << 15)         /* 映射到I/O地址空间 */

/* 权限组合掩码 */
#define VM_ACCESS_FLAGS (VM_READ | VM_WRITE | VM_EXEC)
#define VM_MAYACCESS (VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC | VM_MAYSHARE)

/* 错误码定义 */
#define EINVAL 22 /* 无效参数 */
#define ENOMEM 12 /* 内存不足 */
#define EFAULT 14 /* 错误地址 */
#define EEXIST 17 /* 已存在 */

/* 基本类型定义 */
typedef unsigned long pgd_t;    /* 页全局目录项类型 */
typedef unsigned long pgprot_t; /* 页保护类型 */

/**
 * 将单个物理页映射到虚拟地址
 * @param mm        内存描述符
 * @param vaddr     虚拟地址
 * @param page      物理页结构
 * @param prot      保护标志
 * @return          成功返回0，失败返回错误码
 */
int map_page(struct mm_struct *mm, unsigned long vaddr, struct page *page,
             pgprot_t prot);

/**
 * 取消单个虚拟地址的映射
 * @param mm        内存描述符
 * @param vaddr     虚拟地址
 * @return          成功返回0，失败返回错误码
 */
int unmap_page(struct mm_struct *mm, unsigned long vaddr);

/**
 * 修改单个页面映射的保护属性
 * @param mm        内存描述符
 * @param vaddr     虚拟地址
 * @param prot      新的保护标志
 * @return          成功返回0，失败返回错误码
 */
int protect_page(struct mm_struct *mm, unsigned long vaddr, pgprot_t prot);

/**
 * 查询虚拟地址的物理映射
 * @param mm        内存描述符
 * @param addr      虚拟地址
 * @param pfn       返回的物理页框号
 * @param prot      返回的保护标志
 * @return          成功返回0，失败返回错误码
 */
int query_vm_mapping(struct mm_struct *mm, unsigned long addr,
                     unsigned long *pfn, pgprot_t *prot);

/**
 * 将保护标志转换为页表项标志
 */
pgprot_t vm_get_page_prot(unsigned long vm_flags);

/**
 * 从页表项中提取物理页框号
 */
unsigned long pte_pfn(pte_t pte);

/**
 * 从物理页框号和保护标志构建页表项
 */
pte_t pfn_pte(unsigned long pfn, pgprot_t prot);

pte_t *page_walk(pagetable_t pagetable, uaddr va, int alloc);

#endif /* _LINUX_MM_H */