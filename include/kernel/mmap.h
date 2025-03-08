#ifndef _LINUX_MM_H
#define _LINUX_MM_H

#include <kernel/types.h>
#include <kernel/riscv.h>

/* 页大小相关定义 */
#define PAGE_SHIFT      12
#define PAGE_SIZE       (1UL << PAGE_SHIFT)
#define PAGE_MASK       (~(PAGE_SIZE-1))

/* 页表项标志位定义 */
#define _PAGE_PRESENT   (1UL << 0)  /* 页面存在 */
#define _PAGE_READ      (1UL << 1)  /* 可读 */
#define _PAGE_WRITE     (1UL << 2)  /* 可写 */
#define _PAGE_EXEC      (1UL << 3)  /* 可执行 */
#define _PAGE_USER      (1UL << 4)  /* 用户态可访问 */
#define _PAGE_GLOBAL    (1UL << 5)  /* 全局页 */
#define _PAGE_ACCESSED  (1UL << 6)  /* 已访问 */
#define _PAGE_DIRTY     (1UL << 7)  /* 已修改 */

#define PTE_PFN_MASK    (~(PAGE_SIZE-1))

/* 虚拟内存区域标志位 */
#define VM_READ         (1UL << 0)  /* 可读 */
#define VM_WRITE        (1UL << 1)  /* 可写 */
#define VM_EXEC         (1UL << 2)  /* 可执行 */
#define VM_SHARED       (1UL << 3)  /* 共享映射 */
#define VM_MAYREAD      (1UL << 4)  /* 可设置为可读 */
#define VM_MAYWRITE     (1UL << 5)  /* 可设置为可写 */
#define VM_MAYEXEC      (1UL << 6)  /* 可设置为可执行 */
#define VM_MAYSHARE     (1UL << 7)  /* 可设置为共享 */
#define VM_GROWSDOWN    (1UL << 8)  /* 向下增长(栈) */
#define VM_GROWSUP      (1UL << 9)  /* 向上增长(堆) */
#define VM_USER         (1UL << 10) /* 用户空间 */

/* 错误码定义 */
#define EINVAL          22  /* 无效参数 */
#define ENOMEM          12  /* 内存不足 */
#define EFAULT          14  /* 错误地址 */
#define EEXIST          17  /* 已存在 */

/* 基本类型定义 */
typedef unsigned long pte_t;       /* 页表项类型 */
typedef unsigned long pgd_t;       /* 页全局目录项类型 */
typedef unsigned long pgprot_t;    /* 页保护类型 */

/* 类型转换辅助函数 */
static inline pte_t __pte(unsigned long x)    { return x; }
static inline pgd_t __pgd(unsigned long x)    { return x; }
static inline pgprot_t __pgprot(unsigned long x) { return x; }

/* 值提取辅助函数 */
static inline unsigned long pte_val(pte_t x)  { return x; }
static inline unsigned long pgd_val(pgd_t x)  { return x; }
static inline unsigned long pgprot_val(pgprot_t x) { return x; }

/**
 * 将物理页映射到虚拟地址空间
 * @param mm        内存描述符
 * @param addr      起始虚拟地址
 * @param size      映射大小
 * @param pfn       起始物理页框号
 * @param prot      保护标志
 * @return          成功返回0，失败返回错误码
 */
int map_vm_area(struct mm_struct *mm, unsigned long addr, 
                unsigned long size, unsigned long pfn, pgprot_t prot);

/**
 * 取消虚拟地址空间的映射
 * @param mm        内存描述符
 * @param addr      起始虚拟地址
 * @param size      映射大小
 * @return          成功返回0，失败返回错误码
 */
int unmap_vm_area(struct mm_struct *mm, unsigned long addr, unsigned long size);

/**
 * 修改已有映射的保护属性
 * @param mm        内存描述符
 * @param addr      起始虚拟地址
 * @param size      映射大小
 * @param prot      新的保护标志
 * @return          成功返回0，失败返回错误码
 */
int protect_vm_area(struct mm_struct *mm, unsigned long addr,
                    unsigned long size, pgprot_t prot);

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