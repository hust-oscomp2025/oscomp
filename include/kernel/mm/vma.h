#ifndef _VMA_H
#define _VMA_H

#include <kernel/mm/mm_struct.h>
#include <kernel/types.h>

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
// vm_flags
#define VM_READ (1UL << 0)   /* 可读，PTE_R ，PTE_D */
#define VM_WRITE (1UL << 1)  /* 可写，PTE_W ，PTE_D */
#define VM_EXEC (1UL << 2)   /* 可执行，PTE_R ，PTE_X */
#define VM_SHARED (1UL << 3) /* 共享库、文件、设备、IPC */
#define VM_PRIVATE (1UL << 4) /* 进程私有映射，关系到COW场景的识别 */
#define VM_MAYREAD                                                             \
  (1UL << 5) /* 可设置为可读，主要服务COW和mprotect() 系统调用 */
#define VM_MAYWRITE (1UL << 6) /* 可设置为可写 */
#define VM_MAYEXEC (1UL << 7)  /* 可设置为可执行 */
#define VM_MAYSHARE (1UL << 8) /* 可设置为共享 */
#define VM_GROWSDOWN (1UL << 9) /* 向下增长(栈)，用来在缺页异常中使用 */
#define VM_GROWSUP (1UL << 10) /* 向上增长(堆)，用来在缺页异常中使用 */
#define VM_USER (1UL << 11)       /* 用户空间 ，用来设置 PTE_U */
#define VM_DONTCOPY (1UL << 12)   /* fork时不复制 */
#define VM_DONTEXPAND (1UL << 13) /* 不允许扩展 */
#define VM_LOCKED (1UL << 14) /* 页面锁定，不允许换出（换出到磁盘） */
#define VM_IO (1UL << 15) /* 映射到I/O地址空间，用来标记硬件的MMIO区域 */

/* 权限组合掩码 */
#define VM_ACCESS_FLAGS (VM_READ | VM_WRITE | VM_EXEC)
#define VM_MAYACCESS (VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC | VM_MAYSHARE)
#define VM_USERSTACK                                                           \
  (VM_READ | VM_WRITE | VM_MAYREAD | VM_MAYWRITE | VM_GROWSDOWN | VM_USER |    \
   VM_PRIVATE)

/**
 * 虚拟内存区域 (Virtual Memory Area)
 * 表示进程虚拟地址空间中的连续区域
 */
struct vm_area_struct {
  // 虚拟地址范围
  uint64 vm_start; // 起始虚拟地址
  uint64 vm_end;   // 结束虚拟地址（不包含）

  // 保护标志
  int vm_prot; // 保护标志 (PROT_READ, PROT_WRITE, PROT_EXEC)
  // 蕴含在vm_flags中

  // VMA类型和标志
  enum vma_type vm_type; // VMA类型
  uint64 vm_flags;       // VMA标志位

  // 相关数据结构
  struct mm_struct *vm_mm;  // 所属进程
  struct list_head vm_list; // mm中的vma链表节点

  // 文件映射相关字段
  struct file *vm_file; // 映射的文件（如果是文件映射）
  uint64 vm_pgoff;       // 文件页偏移

  // 页数据
  struct page **pages; // 区域包含的页数组
  int page_count;      // 页数量
  spinlock_t vma_lock; // VMA锁
};

/* VM fault类型及常量定义 */
typedef int vm_fault_t;

#define VM_FAULT_NOPAGE     0x00 /* 页面故障已成功处理 */
#define VM_FAULT_MINOR      0x01 /* 次要页面故障 */
#define VM_FAULT_MAJOR      0x02 /* 主要页面故障 */
#define VM_FAULT_RETRY      0x04 /* 重试页面故障 */
#define VM_FAULT_ERROR      0x08 /* 页面故障处理中发生错误 */
#define VM_FAULT_BADMAP     0x10 /* 错误的映射 */
#define VM_FAULT_BADACCESS  0x20 /* 错误的访问权限 */
#define VM_FAULT_SIGBUS     0x40 /* 总线错误信号 */
#define VM_FAULT_OOM        0x80 /* 内存不足 */

/**
 * 虚拟内存故障信息结构
 * 在页面故障发生时传递相关信息
 */
struct vm_fault {
    /* 发生故障的虚拟地址 */
    uint64 address;

    /* 标志位 */
    unsigned int flags;
    #define FAULT_FLAG_WRITE  0x01 /* 写访问故障 */
    #define FAULT_FLAG_USER   0x02 /* 用户空间访问故障 */
    #define FAULT_FLAG_REMOTE 0x04 /* 远程故障 */
    #define FAULT_FLAG_MKWRITE 0x08 /* 写时复制 */
    #define FAULT_FLAG_ALLOW_RETRY 0x10 /* 允许重试 */
    #define FAULT_FLAG_RETRY_NOWAIT 0x20 /* 不等待重试 */
    #define FAULT_FLAG_KILLABLE 0x40 /* 可被信号杀死 */

    /* 页表项 */
    pte_t *pte;  /* 指向故障页表项的指针 */

    /* 页偏移 */
    uint64 pgoff;

    /* 故障结果页 */
    struct page *page;

    /* 缺页中间状态 */
    int result;
};

struct vm_area_struct *vm_area_setup(struct mm_struct *mm, uint64 addr,
                                     uint64 len, enum vma_type type, int prot,
                                     uint64 flags);

void free_vma(struct vm_area_struct *vma);

int populate_vma(struct vm_area_struct *vma, uint64 addr, size_t length,
                 int prot);

/**
 * @brief 通用页面故障处理函数
 * 
 * @param vma 虚拟内存区域结构
 * @param vmf 页面故障信息
 * @return vm_fault_t 故障处理结果
 */
vm_fault_t handle_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf);

/**
 * @brief 插入页面到虚拟内存区域
 * 
 * @param vma 虚拟内存区域结构
 * @param addr 虚拟地址
 * @param page 要插入的页面
 * @return int 成功返回0，失败返回错误码
 */
int vm_insert_page(struct vm_area_struct *vma, uint64 addr, struct page *page);

#endif