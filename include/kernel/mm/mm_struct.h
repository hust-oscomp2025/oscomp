#ifndef _USER_MEM_H
#define _USER_MEM_H

#include <kernel/types.h>
#include <kernel/mm/page.h>
#include <kernel/mm/pagetable.h>
#include <kernel/process.h>
#include <kernel/riscv.h>
#include <util/list.h>
#include <util/spinlock.h>

/**
 * 虚拟内存区域类型
 */
enum vma_type {
	VMA_ANONYMOUS = 0,  // 匿名映射（如堆）
	VMA_PRIVATE,        // 私有映射
	VMA_SHARED,         // 共享映射
	VMA_FILE,           // 文件映射
	VMA_STACK,          // 栈区域
	VMA_HEAP,           // 堆区域
	VMA_TEXT,           // 代码段
	VMA_DATA,           // 数据段
	VMA_BSS,            // BSS段
	VMA_VDSO            // 虚拟动态共享对象
};

/**
 * 虚拟内存区域 (Virtual Memory Area)
 * 表示进程虚拟地址空间中的连续区域
 */
struct vm_area_struct {
    // 虚拟地址范围
    uaddr vm_start;        // 起始虚拟地址
    uaddr vm_end;          // 结束虚拟地址（不包含）
    
    // 保护标志
    int vm_prot;            // 保护标志 (PROT_READ, PROT_WRITE, PROT_EXEC)
    
    // VMA类型和标志
    enum vma_type vm_type;  // VMA类型
    uint64 vm_flags;        // VMA标志位
    
    // 相关数据结构
    struct mm_struct *vm_mm; // 所属进程
    struct list_head vm_list; // 进程VMA链表节点
    
    // 文件映射相关字段
    struct inode *vm_file;   // 映射的文件（如果是文件映射）
    uint64 vm_pgoff;         // 文件页偏移
    
    // 页数据
    struct page **pages;     // 区域包含的页数组
    int page_count;          // 页数量
    spinlock_t vma_lock;     // VMA锁
};


typedef uint64 pte_t;
typedef uint64* pagetable_t;  // 512 PTEs


/**
 * 用户内存布局
 * 仿照 mm_struct，管理进程的整个地址空间
 */
struct mm_struct {
    // 页表
    pagetable_t pagetable;          // 页表
    // VMA链表
    struct list_head mmap;     // VMA链表头
    int map_count;             // VMA数量
    
    // 地址空间边界
    uaddr start_code;
    uaddr end_code;       // 代码段范围

    uaddr start_data;
    uaddr end_data;       // 数据段范围

    uaddr start_brk;
    uaddr brk;             // 堆范围

    uaddr start_stack;
    uaddr end_stack;     // 栈范围
    
    // 锁和引用计数
    spinlock_t mm_lock;       // mm锁
    atomic_t mm_users;        // 用户数量
    atomic_t mm_count;        // 引用计数
};

/**
 * 将用户虚拟地址转换为物理地址
 *
 * @param mm 内存管理结构体
 * @param user_va 用户空间虚拟地址
 * @return 物理地址，失败返回NULL
 */
inline uint64 mm_lookuppa(struct mm_struct *mm, uaddr user_va) {
  if (!mm || !mm->pagetable){
    return 0;
  }
  return pgt_lookuppa(mm->pagetable, user_va);
}

/**
 * 初始化用户内存管理子系统
 */
void user_mem_init(void);

/**
 * 为进程分配和初始化mm_struct
 * 这是主要的接口函数，类似Linux中的mm_alloc
 * 
 * @param proc 目标进程
 * @return 成功返回0，失败返回负值
 */
int mm_init(struct task_struct *proc);

/**
 * 释放用户内存布局
 */
void user_mm_free(struct mm_struct *mm);

/**
 * 创建新的VMA
 * @param mm 所属的mm结构
 * @param start 起始地址
 * @param end 结束地址
 * @param prot 保护标志
 * @param type VMA类型
 * @param flags VMA标志
 * @return 成功返回VMA指针，失败返回NULL
 */
struct vm_area_struct *create_vma(struct mm_struct *mm, uint64 start, uint64 end, 
                                  int prot, enum vma_type type, uint64 flags);

/**
 * 查找包含指定地址的VMA
 */
struct vm_area_struct *find_vma(struct mm_struct *mm, uint64 addr);

/**
 * 查找与给定范围重叠的VMA
 */
struct vm_area_struct *find_vma_intersection(struct mm_struct *mm, uint64 start, uint64 end);

/**
 * 映射内存区域
 * @param proc 目标进程
 * @param addr 请求的起始地址（如果为0则自动分配）
 * @param length 请求的长度
 * @param prot 保护标志
 * @param type VMA类型
 * @param flags VMA标志
 * @return 成功返回映射的起始地址，失败返回-1
 */
uint64 do_mmap(struct task_struct *proc, uint64 addr, size_t length, int prot, 
               enum vma_type type, uint64 flags);

/**
 * 取消映射内存区域
 * @param proc 目标进程
 * @param addr 起始地址
 * @param length 长度
 * @return 成功返回0，失败返回-1
 */
int do_munmap(struct task_struct *proc, uint64 addr, size_t length);

/**
 * 分配一个页并映射到指定地址
 * @param proc 目标进程
 * @param addr 虚拟地址
 * @param prot 保护标志
 * @return 成功返回映射的地址，失败返回NULL
 */
void *mm_user_alloc_page(struct task_struct *proc, uaddr addr, int prot);


/**
 * 扩展堆
 * @param proc 目标进程
 * @param increment 增加或减少的字节数
 * @return 成功返回新的brk地址，失败返回-1
 */
uint64 do_brk(struct task_struct *proc, int64 increment);

/**
 * 分配特定数量的页到用户空间
 * @param proc 目标进程
 * @param nr_pages 页数量
 * @param addr 请求的起始地址（如果为0则自动分配）
 * @param prot 保护标志
 * @return 成功返回分配的起始地址，失败返回NULL
 */
void *user_alloc_pages(struct task_struct *proc, int nr_pages, uint64 addr, int prot);

/**
 * 从用户空间释放页
 * @param proc 目标进程
 * @param addr 起始地址
 * @param nr_pages 页数量
 * @return 成功返回0，失败返回-1
 */
int user_free_pages(struct task_struct *proc, uint64 addr, int nr_pages);

/**
 * 安全地将数据复制到用户空间
 * @param proc 目标进程
 * @param dst 目标用户空间地址
 * @param src 源内核空间地址
 * @param len 复制长度
 * @return 成功复制的字节数，失败返回-1
 */
ssize_t copy_to_user(struct task_struct *proc, void *dst, const void *src, size_t len);

/**
 * 安全地从用户空间复制数据
 * @param proc 源进程
 * @param dst 目标内核空间地址
 * @param src 源用户空间地址
 * @param len 复制长度
 * @return 成功复制的字节数，失败返回-1
 */
ssize_t copy_from_user(struct task_struct *proc, void *dst, const void *src, size_t len);

/**
 * 打印进程的内存布局信息，用于调试
 */
void print_proc_memory_layout(struct task_struct *proc);

#endif /* _USER_MEM_H */