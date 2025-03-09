/**
 * @file pagetable.h
 * @brief RISC-V SV39 页表管理模块
 *
 * 该模块提供了RISC-V SV39分页方案下的页表管理功能，
 * 包括页表的创建、映射、查找和释放等操作。
 */

#ifndef _PAGETABLE_H
#define _PAGETABLE_H

#include <util/atomic.h>
#include <kernel/riscv.h>
#include <util/spinlock.h>
#include <kernel/types.h>
#include <kernel/mm/page.h>

/**
 * @brief 页表管理数据结构
 */
typedef uint64 pte_t;       // 页表项类型
typedef pte_t *pagetable_t; // 页表类型(指向512个PTE的数组)

/**
 * @brief SV39页表常量定义
 */
// SV39虚拟地址布局: 39位虚拟地址, 分为3级页表
#define SATP_MODE_SV39 8L
#define MAKE_SATP(pagetable)                                                   \
  (((uint64)SATP_MODE_SV39 << 60) | (((uint64)pagetable) >> 12))
#define VA_BITS 39
#define PAGE_LEVELS 3

#define PTE_V (1L << 0) // valid
#define PTE_R (1L << 1) // readable
#define PTE_W (1L << 2) // writable
#define PTE_X (1L << 3) // executable
#define PTE_U (1L << 4) // 1->user can access, 0->otherwise
#define PTE_G (1L << 5) // global
#define PTE_A (1L << 6) // accessed
#define PTE_D (1L << 7) // dirty

// 页表项(PTE)常量
#define PTE_PPN_SHIFT 10     // PPN字段在PTE中的位移
#define PTE_FLAGS_MASK 0x3FF // 低10位是标志位
#define PTE_FLAGS(pte) ((pte) & PTE_FLAGS_MASK)

// shift a physical address to the right place for a PTE.
#define PA2PPN(pa) ((((uint64)pa) >> PAGE_SHIFT) << PTE_PPN_SHIFT)

// convert a pte content into its corresponding physical address
#define PTE2PA(pte) (((pte) >> PTE_PPN_SHIFT) << PAGE_SHIFT)
// extract the three 9-bit page table indices from a virtual address.
// 页表级别常量和掩码
#define PT_INDEX_MASK 0x1FF // 每级页表索引为9位
#define PT_INDEX_BITS 9
#define PT_ENTRIES (1 << PT_INDEX_BITS) // 每个页表512个条目
#define PXSHIFT(level) (PAGE_SHIFT + (PT_INDEX_BITS * (level)))
#define PX(level, va) ((((uint64)(va)) >> PXSHIFT(level)) & PT_INDEX_MASK)

/* 页表项标志位定义 */
#define _PAGE_PRESENT   (1UL << 0)  /* 页面存在 */
#define _PAGE_READ      (1UL << 1)  /* 可读 */
#define _PAGE_WRITE     (1UL << 2)  /* 可写 */
#define _PAGE_EXEC      (1UL << 3)  /* 可执行 */
#define _PAGE_USER      (1UL << 4)  /* 用户态可访问 */
#define _PAGE_GLOBAL    (1UL << 5)  /* 全局页 */
#define _PAGE_ACCESSED  (1UL << 6)  /* 已访问 */
#define _PAGE_DIRTY     (1UL << 7)  /* 已修改 */



/**
 * @brief 页表统计信息结构
 */
typedef struct {
  atomic_t mapped_pages; // 已映射的页数
  atomic_t page_tables;  // 页表数量
} pagetable_stats_t;



/**
 * @brief 初始化页表子系统
 */
void pagetable_init(void);

/**
 * @brief 创建一个新的空页表
 *
 * @return pagetable_t 返回新页表的虚拟地址指针，失败时返回NULL
 */
pagetable_t pagetable_create(void);

/**
 * @brief 释放整个页表结构(包括所有级别的页表)
 *
 * @param pagetable 要释放的页表
 */
void pagetable_free(pagetable_t pagetable);

/**
 * @brief 在页表中映射虚拟地址到物理地址，可能会分配新的页表页
 *
 * @param pagetable 页表
 * @param va 虚拟地址(页对齐)
 * @param pa 物理地址(页对齐)
 * @param size 要映射的字节数(将四舍五入到页的大小)
 * @param perm 权限标志(PTE_R, PTE_W, PTE_X, PTE_U等)
 * @return int 成功返回0，失败返回负值
 */
int pgt_map_page(pagetable_t pagetable, uaddr va, uint64 pa,
                  int perm);

/**
 * @brief 解除页表中一块虚拟地址区域的映射
 *
 * @param pagetable 页表
 * @param va 虚拟地址起始点(页对齐)
 * @param size 要取消映射的字节数(将四舍五入到页的大小)
 * @param free_phys 是否同时释放物理页内存
 * @return int 成功返回0，失败返回负值
 */
int pgt_unmap(pagetable_t pagetable, uaddr va, uint64 size,
                    int free_phys);

/**
 * @brief 在页表中查找页表项
 *
 * @param pagetable 页表
 * @param va 虚拟地址
 * @param alloc 如果为1，则在页表不存在时分配一个新页表页
 * @return pte_t* 返回PTE的地址，如果不存在且alloc=0，则返回NULL
 */
pte_t *pgt_walk(pagetable_t pagetable, uaddr va, int alloc);

/**
 * @brief 查找虚拟地址对应的物理地址
 *
 * @param pagetable 页表
 * @param va 虚拟地址
 * @return uint64 物理地址，如果映射不存在则返回0
 */
uint64 pgt_lookuppa(pagetable_t pagetable, uaddr va);

/**
 * @brief 复制页表结构(可选择是否复制映射)
 *
 * @param src 源页表
 * @param start 起始虚拟地址
 * @param end 结束虚拟地址
 * @param share 映射类型: 0=复制物理页, 1=共享物理页, 2=写时复制
 * @return pagetable_t 复制的页表，失败返回NULL
 */
pagetable_t pagetable_copy(pagetable_t src, uaddr start, uaddr end, int share);

/**
 * @brief 打印页表内容(用于调试)
 *
 * @param pagetable 页表
 */
void pagetable_dump(pagetable_t pagetable);

/**
 * @brief 设置satp寄存器，激活指定页表
 *
 * @param pagetable 要激活的页表
 */
void pagetable_activate(pagetable_t pagetable);

/**
 * @brief 获取当前活动的页表
 *
 * @return pagetable_t 当前页表
 */
pagetable_t pagetable_current(void);

void check_address_mapping(pagetable_t pagetable, uint64 va);

extern pagetable_stats_t pt_stats; // 全局页表统计信息
// pointer to kernel page directory
extern pagetable_t g_kernel_pagetable;

#endif /* _PAGETABLE_H */