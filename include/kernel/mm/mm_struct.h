#ifndef _USER_MEM_H
#define _USER_MEM_H

#include <kernel/mm/mmap.h>
#include <kernel/mm/page.h>
#include <kernel/mm/pagetable.h>
#include <kernel/mm/vma.h>
#include <kernel/riscv.h>
#include <kernel/sched/process.h>
#include <kernel/types.h>
#include <util/list.h>
#include <util/spinlock.h>

typedef uint64 pte_t;
typedef uint64 *pagetable_t; // 512 PTEs
enum vma_type;
/**
 * 用户内存布局
 * 仿照 mm_struct，管理进程的整个地址空间
 */
struct mm_struct {
  // struct maple_tree mm_mt;
  int is_kernel_mm;
  // 页表
  pagetable_t pagetable; // 页表
  // VMA链表
  struct list_head vma_list; // VMA链表头
  int map_count;             // VMA数量

  // 地址空间边界
  uint64 start_code;
  uint64 end_code; // 代码段范围

  uint64 start_data;
  uint64 end_data; // 数据段范围

  uint64 start_brk;
  uint64 brk; // 堆范围

  uint64 start_stack;
  uint64 end_stack; // 栈范围

  // 锁和引用计数
  spinlock_t mm_lock; // mm锁
  atomic_t mm_users;  // 用户数量
  atomic_t mm_count;  // 引用计数
};

// 初始化内核mm结构
void create_init_mm();

struct mm_struct *user_alloc_mm(void);
void free_mm(struct mm_struct *mm);

uint64 prot_to_type(int prot, int user);

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
                                  enum vma_type type, uint64 flags);

/**
 * 查找包含指定地址的VMA
 */
struct vm_area_struct *find_vma(struct mm_struct *mm, uint64 addr);

/**
 * 查找与给定范围重叠的VMA
 */
struct vm_area_struct *find_vma_intersection(struct mm_struct *mm, uint64 start,
	uint64 end);

/**
 * 扩展堆
 * @param proc 目标进程
 * @param increment 增加或减少的字节数
 * @return 成功返回新的brk地址，失败返回-1
 */
uint64 mm_brk(struct mm_struct *mm, int64 increment);

/**
 * 安全地将数据复制到用户空间
 * @param proc 目标进程
 * @param dst 目标用户空间地址
 * @param src 源内核空间地址
 * @param len 复制长度
 * @return 成功复制的字节数，失败返回-1
 */
ssize_t mm_copy_to_user(struct mm_struct *proc, uint64 dst, const void* src,
                        size_t len);

/**
 * 安全地从用户空间复制数据
 * @param proc 源进程
 * @param dst 目标内核空间地址
 * @param src 源用户空间地址
 * @param len 复制长度
 * @return 成功复制的字节数，失败返回-1
 */
ssize_t mm_copy_from_user(struct mm_struct *proc, uint64 dst, const void* src,
                          size_t len);

#endif /* _USER_MEM_H */