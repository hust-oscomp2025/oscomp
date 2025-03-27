#include <kernel/types.h>

#include <kernel/mm/kmalloc.h>
#include <kernel/mm/mm_struct.h>
#include <kernel/mm/mmap.h>
#include <kernel/mm/page.h>
#include <kernel/mm/vma.h>
#include <kernel/sched/process.h>
#include <kernel/sprint.h>
#include <kernel/util/atomic.h>
#include <kernel/util/string.h>

// user_alloc_mm
struct mm_struct *user_alloc_mm(void) {

  // 创建mm结构
  struct mm_struct *mm = (struct mm_struct *)kmalloc(sizeof(struct mm_struct));

  // 初始化mm结构
  memset(mm, 0, sizeof(struct mm_struct));
  INIT_LIST_HEAD(&mm->vma_list);
  mm->map_count = 0;

  mm->is_kernel_mm = 0;
  // 记录页表的内核虚拟地址
  mm->pagetable = (pagetable_t)kmalloc(PAGE_SIZE);
  if (unlikely(mm->pagetable == NULL)) {
    sprint("alloc_mm: kmalloc failed\n");
    return NULL;
  }
  memset(mm->pagetable, 0, PAGE_SIZE);

  spinlock_init(&mm->mm_lock);
  atomic_set(&mm->mm_users, 1);
  atomic_set(&mm->mm_count, 1);

  // 用户空间默认布局
  mm->start_code = (uint64 )0x00400000;     // 代码段默认起始地址
  mm->end_code = (uint64 )0x00400000;       // 初始时代码段为空
  mm->start_data = (uint64 )0x10000000;     // 数据段默认起始地址
  mm->end_data = (uint64 )0x10000000;       // 初始时数据段为空
  mm->start_brk = USER_FREE_ADDRESS_START; // 堆默认起始地址
  mm->brk = USER_FREE_ADDRESS_START;       // 初始时堆为空

  mm->start_stack = USER_STACK_TOP - PAGE_SIZE; // 栈默认起始地址
  mm->end_stack = USER_STACK_TOP;               // 栈默认结束地址

  struct vm_area_struct *stack_vma =
      vm_area_setup(mm, mm->start_stack, mm->end_stack - mm->start_stack,
                    VMA_STACK, PROT_READ | PROT_WRITE, VM_USERSTACK);
  // 有VM_GROWSDOWN flag，可以动态向下增长

  // 分配并映射初始栈页
  if (unlikely(stack_vma == NULL)) {
    sprint("alloc_mm: failed to allocate initial stack page\n");
    return NULL;
  }

  return mm;
}

/**
 * 释放用户内存布局
 */
void free_mm(struct mm_struct *mm) {
  if (!mm)
    return;

  // 减少引用计数
  if (atomic_dec_and_test(&mm->mm_count)) {
    // 引用计数为0，可以释放mm结构

    // 释放所有VMA
    struct vm_area_struct *vma, *tmp;
    list_for_each_entry_safe(vma, tmp, &mm->vma_list, vm_list) {
      free_vma(vma);
      // 释放VMA关联的所有页
      if (vma->pages) {
        for (int32 i = 0; i < vma->page_count; i++) {
          if (vma->pages[i]) {
            put_page(vma->pages[i]);
          }
        }
        kfree(vma->pages);
      }

      // 从链表中移除并释放VMA
      list_del(&vma->vm_list);
      kfree(vma);
    }

    // 释放页表
    if (mm->pagetable) {
      free_pagetable(mm->pagetable);
    }

    // 释放mm结构
    kfree(mm);
  }
}

/**
 * 查找包含指定地址的VMA
 */
struct vm_area_struct *find_vma(struct mm_struct *mm, uint64 addr) {
  if (!mm)
    return NULL;

  struct vm_area_struct *vma;
  list_for_each_entry(vma, &mm->vma_list, vm_list) {
    if (addr >= vma->vm_start && addr < vma->vm_end)
      return vma;
  }

  return NULL;
}

/**
 * 查找与给定范围重叠的VMA
 * find_vma_intersection
 */
struct vm_area_struct *find_vma_intersection(struct mm_struct *mm, uint64 start,
                                             uint64 end) {
  if (!mm || start >= end)
    return NULL;

  struct vm_area_struct *vma;
  list_for_each_entry(vma, &mm->vma_list, vm_list) {
    // 检查是否有重叠
    if (start < vma->vm_end && end > vma->vm_start)
      return vma;
  }

  return NULL;
}
/**
 * mm_copy_to_user - Internal implementation of copy to user
 * @mm:   The memory descriptor of target process
 * @dst:  Destination address in user space
 * @src:  Source address in kernel space
 * @len:  Number of bytes to copy
 *
 * Returns number of bytes copied on success, negative error code on failure
 */
ssize_t mm_copy_to_user(struct mm_struct *mm, uint64 dst, const void* src,
                        size_t len) {
  if (!mm || !dst || !src || len == 0)
    return -EINVAL;

  uint64 dst_addr = dst;
  const char *src_ptr = (const char *)src;
  size_t bytes_copied = 0;

  while (bytes_copied < len) {
    // Find the VMA for current address
    struct vm_area_struct *vma = find_vma(mm, dst_addr + bytes_copied);
    if (!vma)
      return bytes_copied > 0 ? bytes_copied : -EFAULT;

    // Check write permission
    if (!(vma->vm_prot & PROT_WRITE))
      return bytes_copied > 0 ? bytes_copied : -EFAULT;

    // Calculate bytes to copy in current page
    uint64 page_offset = (dst_addr + bytes_copied) | (PAGE_SIZE - 1);
    uint64 page_bytes = MIN(PAGE_SIZE - page_offset, len - bytes_copied);

    // Get page virtual address
    uint64 page_va = ROUNDDOWN(dst_addr + bytes_copied, PAGE_SIZE);

    // Find page index
    int32 page_idx = (page_va - vma->vm_start) / PAGE_SIZE;
    if (page_idx < 0 || page_idx >= vma->page_count)
      return bytes_copied > 0 ? bytes_copied : -EFAULT;

    // Ensure page is allocated
    if (!vma->pages[page_idx]) {
      populate_vma(vma, page_va, PAGE_SIZE, vma->vm_prot);
    }

    // Calculate target address (kernel view)
    char *target = (char *)vma->pages[page_idx]->paddr + page_offset;

    // Copy data
    memcpy(target, src_ptr + bytes_copied, page_bytes);
    bytes_copied += page_bytes;
  }

  return bytes_copied;
}

/**
 * 安全地从用户空间复制数据
 */
ssize_t mm_copy_from_user(struct mm_struct *mm, uint64 dst, const void* src,
                          size_t len) {
  if (!mm || !dst || !src || len == 0)
    return -EINVAL;

  uint64 src_addr = (uint64)src;
  char *dst_ptr = (char *)dst;
  size_t bytes_copied = 0;

  while (bytes_copied < len) {
    // 查找当前地址所在的VMA
    struct vm_area_struct *vma = find_vma(mm, src_addr + bytes_copied);
    if (!vma)
      return bytes_copied > 0 ? bytes_copied : -1;

    // 检查读权限
    if (!(vma->vm_prot & PROT_READ))
      return bytes_copied > 0 ? bytes_copied : -1;

    // 计算当前页内的复制长度
    uint64 page_offset = (src_addr + bytes_copied) % PAGE_SIZE;
    uint64 page_bytes = MIN(PAGE_SIZE - page_offset, len - bytes_copied);

    // 获取源页的虚拟地址
    uint64 page_va = ROUNDDOWN(src_addr + bytes_copied, PAGE_SIZE);

    // 查找对应的页结构
    int32 page_idx = (page_va - vma->vm_start) / PAGE_SIZE;
    if (page_idx < 0 || page_idx >= vma->page_count)
      return bytes_copied > 0 ? bytes_copied : -1;

    // 确保页已分配
    if (!vma->pages[page_idx]) {
      // 页未分配，对于读操作这是错误
      return bytes_copied > 0 ? bytes_copied : -1;
    }

    // 计算实际源地址（内核视角）
    const char *source = (const char *)vma->pages[page_idx]->paddr + page_offset;

    // 复制数据
    memcpy(dst_ptr + bytes_copied, source, page_bytes);
    bytes_copied += page_bytes;
  }

  return bytes_copied;
}
