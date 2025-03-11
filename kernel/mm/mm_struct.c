#include <errno.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/mm/mm_struct.h>
#include <kernel/mm/mmap.h>
#include <kernel/mm/page.h>
#include <kernel/process.h>
#include <spike_interface/spike_utils.h>
#include <util/atomic.h>
#include <util/string.h>

/**
 * 初始化用户内存管理子系统
 */
void user_mem_init(void) {
  // 初始化可能的全局资源
  sprint("User memory management subsystem initialized\n");
}

/**
 * 为进程分配和初始化mm_struct
 * 这是主要的接口函数，类似Linux中的mm_alloc
 *
 * @param proc 目标进程
 * @return 成功返回0，失败返回负值
 */
int mm_init(struct task_struct *proc) {
  if (!proc)
    return -1;

  // 创建mm结构
  struct mm_struct *mm = (struct mm_struct *)kmalloc(sizeof(struct mm_struct));
  if (!mm)
    return -1;

  // 初始化mm结构
  memset(mm, 0, sizeof(struct mm_struct));
  INIT_LIST_HEAD(&mm->vma_list);
  mm->map_count = 0;
  spinlock_init(&mm->mm_lock);
  atomic_set(&mm->mm_users, 1);
  atomic_set(&mm->mm_count, 1);

  // 用户空间默认布局
  mm->start_code = 0x00400000;                  // 代码段默认起始地址
  mm->end_code = 0x00400000;                    // 初始时代码段为空
  mm->start_data = 0x10000000;                  // 数据段默认起始地址
  mm->end_data = 0x10000000;                    // 初始时数据段为空
  mm->start_brk = USER_FREE_ADDRESS_START;      // 堆默认起始地址
  mm->brk = USER_FREE_ADDRESS_START;            // 初始时堆为空
  mm->start_stack = USER_STACK_TOP - PAGE_SIZE; // 栈默认起始地址
  mm->end_stack = USER_STACK_TOP;               // 栈默认结束地址

  // 分配页表
  struct page *page = alloc_page();
  if (!page) {
    kfree(mm);
    return -1;
  }

  mm->pagetable = page->virtual_address;
  proc->mm = mm;

  // 创建初始栈区域VMA
  struct vm_area_struct *stack_vma =
      create_vma(mm, mm->start_stack, mm->end_stack, PROT_READ | PROT_WRITE,
                 VMA_STACK, VM_GROWSDOWN | VM_PRIVATE);
  if (!stack_vma) {
    user_mm_free(mm);
    proc->mm = NULL;
    return -1;
  }

  // 分配并映射初始栈页
  void *page_addr =
      mm_alloc_page(proc, mm->start_stack, PROT_READ | PROT_WRITE);
  if (!page_addr) {
    user_mm_free(mm);
    proc->mm = NULL;
    return -1;
  }

  return 0;
}

/**
 * 释放用户内存布局
 */
void user_mm_free(struct mm_struct *mm) {
  if (!mm)
    return;

  // 减少引用计数
  if (atomic_dec_and_test(&mm->mm_count)) {
    // 引用计数为0，可以释放mm结构

    // 释放所有VMA
    struct vm_area_struct *vma, *tmp;
    list_for_each_entry_safe(vma, tmp, &mm->vma_list, vm_list) {
      // 释放VMA关联的所有页
      if (vma->pages) {
        for (int i = 0; i < vma->page_count; i++) {
          if (vma->pages[i]) {
            free_page(vma->pages[i]);
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
      pagetable_free(mm->pagetable);
    }

    // 释放mm结构
    kfree(mm);
  }
}

/**
 * 创建新的VMA
 */
struct vm_area_struct *create_vma(struct mm_struct *mm, uint64 start,
                                  uint64 end, int prot, enum vma_type type,
                                  uint64 flags) {
  if (!mm || start >= end)
    return NULL;

  // 检查是否与现有VMA重叠
  if (find_vma_intersection(mm, start, end))
    return NULL;

  // 分配VMA结构
  struct vm_area_struct *vma =
      (struct vm_area_struct *)kmalloc(sizeof(struct vm_area_struct));
  if (!vma)
    return NULL;

  // 初始化VMA
  memset(vma, 0, sizeof(struct vm_area_struct));
  vma->vm_start = start;
  vma->vm_end = end;
  vma->vm_prot = prot;
  vma->vm_type = type;
  vma->vm_flags = flags;
  vma->vm_mm = mm;

  spinlock_init(&vma->vma_lock);

  // 计算需要的页数
  vma->page_count = (end - start + PAGE_SIZE - 1) / PAGE_SIZE;

  // 分配页数组
  if (vma->page_count > 0) {
    vma->pages =
        (struct page **)kmalloc(vma->page_count * sizeof(struct page *));
    if (!vma->pages) {
      kfree(vma);
      return NULL;
    }
    memset(vma->pages, 0, vma->page_count * sizeof(struct page *));
  }

  // 添加到mm的VMA链表
  list_add(&vma->vm_list, &mm->vma_list);
  mm->map_count++;

  return vma;
}

/**
 * 查找包含指定地址的VMA
 */
struct vm_area_struct *mm_find_vma(struct mm_struct *mm, uint64 addr) {
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
 * 映射内存区域
 */
uint64 mm_map_pages(struct mm_struct *mm, uint64 va, uint64 pa, size_t length,
                    int prot, enum vma_type type, uint64 flags) {
  if (!mm || length == 0)
    return -EINVAL;

  // 对齐长度到页大小
  length = ROUNDUP(length, PAGE_SIZE);

  // 如果未指定地址，则自动分配
  if (va == 0) {
    // 简单策略：从堆区域之后查找空闲区域
    va = mm->brk;

    // 检查是否与现有区域重叠
    while (find_vma_intersection(mm, va, va + length)) {
      va += PAGE_SIZE;
    }
  } else {
    // 检查指定地址是否可用
    if (find_vma_intersection(mm, va, va + length))
      return -1;
  }

  // 创建VMA
  struct vm_area_struct *vma =
      create_vma(mm, va, va + length, prot, type, flags);
  if (!vma)
    return -1;

  // 理论上来说，要实现懒分配（待实现）

  if (type != VMA_ANONYMOUS || (flags & MAP_POPULATE)) {
    for (uint64 off = 0; off < length; off += PAGE_SIZE) {
      pgt_map_page(mm->pagetable, va + off, pa + off, prot);
    }
  }

  // 可能会和错误码冲突
  return va;
}

/**
 * 取消映射内存区域
 */
int mm_unmap(struct task_struct *proc, uint64 addr, size_t length) {
  if (!proc || !proc->mm || length == 0)
    return -1;

  struct mm_struct *mm = proc->mm;

  // 对齐到页大小
  addr = ROUNDDOWN(addr, PAGE_SIZE);
  length = ROUNDUP(length, PAGE_SIZE);
  uint64 end = addr + length;

  // 查找并处理所有受影响的VMA
  struct vm_area_struct *vma, *tmp;
  list_for_each_entry_safe(vma, tmp, &mm->vma_list, vm_list) {
    // 检查是否有重叠
    if (addr < vma->vm_end && end > vma->vm_start) {
      // 计算重叠区域
      uaddr overlap_start = MAX(addr, vma->vm_start);
      uaddr overlap_end = MIN(end, vma->vm_end);

      // 计算页索引
      int start_idx = (overlap_start - vma->vm_start) / PAGE_SIZE;
      int end_idx = (overlap_end - vma->vm_start) / PAGE_SIZE;

      // 释放和取消映射重叠区域的页
      for (int i = start_idx; i < end_idx; i++) {
        if (vma->pages[i]) {
          // 取消对应虚拟地址的映射
          uint64 page_va = vma->vm_start + (i * PAGE_SIZE);
          pgt_unmap(mm->pagetable, page_va, PAGE_SIZE, 1);

          // 释放页
          free_page(vma->pages[i]);
          vma->pages[i] = NULL;
        }
      }

      // 处理VMA裁剪或删除
      if (overlap_start <= vma->vm_start && overlap_end >= vma->vm_end) {
        // 完全覆盖，删除整个VMA
        list_del(&vma->vm_list);
        if (vma->pages)
          kfree(vma->pages);
        kfree(vma);
        mm->map_count--;
      } else if (overlap_start > vma->vm_start && overlap_end < vma->vm_end) {
        // 中间部分被取消映射，需要分割为两个VMA
        // 这种情况较复杂，简化处理：保留前段，创建后段

        // 创建后段VMA
        uint64 new_start = overlap_end;
        uint64 new_end = vma->vm_end;
        struct vm_area_struct *new_vma = create_vma(
            mm, new_start, new_end, vma->vm_prot, vma->vm_type, vma->vm_flags);
        if (!new_vma)
          return -1;

        // 复制页引用
        int new_start_idx = (new_start - vma->vm_start) / PAGE_SIZE;
        for (int i = 0; i < new_vma->page_count; i++) {
          if (new_start_idx + i < vma->page_count) {
            new_vma->pages[i] = vma->pages[new_start_idx + i];
            vma->pages[new_start_idx + i] = NULL;
          }
        }

        // 调整原VMA的结束地址
        vma->vm_end = overlap_start;
      } else if (overlap_start <= vma->vm_start) {
        // 前段被取消映射
        vma->vm_start = overlap_end;

        // 移动页引用
        int move_count = vma->page_count - end_idx;
        if (move_count > 0) {
          memmove(&vma->pages[0], &vma->pages[end_idx],
                  move_count * sizeof(struct page *));
          memset(&vma->pages[move_count], 0,
                 (vma->page_count - move_count) * sizeof(struct page *));
        }
      } else {
        // 后段被取消映射
        vma->vm_end = overlap_start;

        // 清除后段页引用
        memset(&vma->pages[start_idx], 0,
               (vma->page_count - start_idx) * sizeof(struct page *));
      }
    }
  }

  return 0;
}

/**
 * 分配一个页并映射到指定地址
 */
void *mm_alloc_page(struct mm_struct *mm, uaddr addr, int prot) {
  if (!mm || !mm->pagetable)
    return NULL;

  // 分配一个页结构体
  struct page *page = alloc_page();
  if (!page)
    return NULL;

  // 获取页结构对应的物理地址
  void *pa = page_to_virt(page);

  // 映射到用户虚拟地址空间
  int result =
      pgt_map_page(mm->pagetable, addr, (uint64)pa, prot_to_type(prot, 1));

  if (result != 0) {
    // 映射失败，释放页面
    free_page(page);
    return NULL;
  }

  // 记录映射关系
  if (mm) {
    struct vm_area_struct *vma = mm_find_vma(mm, addr);
    if (vma) {
      int page_idx = (addr - vma->vm_start) / PAGE_SIZE;
      if (page_idx >= 0 && page_idx < vma->page_count) {
        vma->pages[page_idx] = page;
      }
    }
  }

  // 设置页的相关属性
  page->virtual_address = (void *)addr;

  // 返回虚拟地址
  return (void *)addr;
}

/**
 * 扩展堆
 */
uint64 mm_brk(struct mm_struct *mm, int64 increment) {
  if (!mm)
    return -EINVAL;

  if (increment == 0)
    return mm->brk; // 仅查询当前brk

  uint64 new_brk = mm->brk + increment;

  // 验证新brk是否有效
  if (new_brk < mm->start_brk)
    return -1; // 不能低于起始堆地址

  // 检查堆是否与其他区域重叠
  struct vm_area_struct *vma = mm_find_vma(mm, new_brk - 1);
  if (vma && vma->vm_start < new_brk && vma->vm_type != VMA_HEAP)
    return -1; // 与非堆区域重叠

  if (increment > 0) {
    // 扩展堆
    vma = mm_find_vma(mm, mm->brk);
    if (!vma || vma->vm_type != VMA_HEAP) {
      // 需要创建新的堆VMA
      vma = create_vma(mm, mm->brk, new_brk, PROT_READ | PROT_WRITE, VMA_HEAP,
                       VM_GROWSUP | VM_PRIVATE);
      if (!vma)
        return -1;
    } else {
      // 扩展现有堆VMA
      vma->vm_end = new_brk;

      // 如果需要，重新分配页数组
      int new_page_count =
          (new_brk - vma->vm_start + PAGE_SIZE - 1) / PAGE_SIZE;
      if (new_page_count > vma->page_count) {
        struct page **new_pages =
            (struct page **)kmalloc(new_page_count * sizeof(struct page *));
        if (!new_pages)
          return -1;

        // 复制旧页引用并扩展
        memset(new_pages, 0, new_page_count * sizeof(struct page *));
        if (vma->pages) {
          memcpy(new_pages, vma->pages,
                 vma->page_count * sizeof(struct page *));
          kfree(vma->pages);
        }

        vma->pages = new_pages;
        vma->page_count = new_page_count;
      }
    }
  } else {
    // 收缩堆
    uint64 old_brk = mm->brk;

    // 查找并调整堆VMA
    vma = mm_find_vma(mm, mm->brk - 1);
    if (vma && vma->vm_type == VMA_HEAP) {
      // 释放不再使用的页
      int start_idx = (new_brk - vma->vm_start + PAGE_SIZE - 1) / PAGE_SIZE;
      int end_idx = (old_brk - vma->vm_start + PAGE_SIZE - 1) / PAGE_SIZE;

      for (int i = start_idx; i < end_idx; i++) {
        if (vma->pages[i]) {
          uint64 page_va = vma->vm_start + (i * PAGE_SIZE);
          pgt_unmap(mm->pagetable, page_va, PAGE_SIZE, 1);
          free_page(vma->pages[i]);
          vma->pages[i] = NULL;
        }
      }

      // 调整VMA结束地址
      vma->vm_end = new_brk;
    }
  }

  // 更新brk
  mm->brk = new_brk;
  return new_brk;
}

/**
 * 分配特定数量的页到用户空间
 */
void *mm_alloc_pages(struct mm_struct *mm, int nr_pages, uint64 addr,
                     int prot) {
  if (!mm || nr_pages <= 0)
    return -EINVAL;

  // 计算所需空间大小
  size_t length = nr_pages * PAGE_SIZE;

  // 分配虚拟地址空间
  uint64 mmap_addr =
      mm_map_pages(mm, 0, addr, length, prot, VMA_ANONYMOUS, VM_PRIVATE);
  if (mmap_addr == (uint64)-1)
    return NULL;

  // 预分配所有页并立即映射（不使用按需分页）
  for (int i = 0; i < nr_pages; i++) {
    void *page_addr = mm_alloc_page(mm, mmap_addr + (i * PAGE_SIZE), prot);
    if (!page_addr) {
      // 分配失败，释放已分配的页
      mm_unmap(mm, mmap_addr, i * PAGE_SIZE);
      return NULL;
    }
  }

  return (void *)mmap_addr;
}

/**
 * 从用户空间释放页
 */
int mm_free_pages(struct mm_struct *mm, uint64 addr, int nr_pages) {
  if (!mm || nr_pages <= 0)
    return -1;

  // 计算释放区域大小
  size_t length = nr_pages * PAGE_SIZE;

  // 取消映射并释放页
  return mm_unmap(mm, addr, length);
}

/**
 * 安全地将数据复制到用户空间
 */
ssize_t mm_copy_to_user(struct mm_struct *mm, void *dst, const void *src,
                        size_t len) {
  if (!mm || !dst || !src || len == 0)
    return -EINVAL;

  uint64 dst_addr = (uint64)dst;
  const char *src_ptr = (const char *)src;
  size_t bytes_copied = 0;

  while (bytes_copied < len) {
    // 查找当前地址所在的VMA
    struct vm_area_struct *vma = mm_find_vma(mm, dst_addr + bytes_copied);
    if (!vma)
      return bytes_copied > 0 ? bytes_copied : -1;

    // 检查写权限
    if (!(vma->vm_prot & PROT_WRITE))
      return bytes_copied > 0 ? bytes_copied : -1;

    // 计算当前页内的复制长度
    uint64 page_offset = (dst_addr + bytes_copied) % PAGE_SIZE;
    uint64 page_bytes = MIN(PAGE_SIZE - page_offset, len - bytes_copied);

    // 获取目标页的虚拟地址
    uint64 page_va = ROUNDDOWN(dst_addr + bytes_copied, PAGE_SIZE);

    // 查找对应的页结构
    int page_idx = (page_va - vma->vm_start) / PAGE_SIZE;
    if (page_idx < 0 || page_idx >= vma->page_count)
      return bytes_copied > 0 ? bytes_copied : -1;

    // 确保页已分配
    if (!vma->pages[page_idx]) {
      void *page_addr = mm_alloc_page(mm, page_va, vma->vm_prot);
      if (!page_addr)
        return bytes_copied > 0 ? bytes_copied : -1;
    }

    // 获取页的物理地址
    void *pa = page_to_virt(vma->pages[page_idx]);
    if (!pa)
      return bytes_copied > 0 ? bytes_copied : -1;

    // 计算实际目标地址（内核视角）
    char *target = (char *)pa + page_offset;

    // 复制数据
    memcpy(target, src_ptr + bytes_copied, page_bytes);
    bytes_copied += page_bytes;
  }

  return bytes_copied;
}

/**
 * 安全地从用户空间复制数据
 */
ssize_t mm_copy_from_user(struct mm_struct *mm, void *dst, const void *src,
                          size_t len) {
  if (!mm || !dst || !src || len == 0)
    return -EINVAL;

  uint64 src_addr = (uint64)src;
  char *dst_ptr = (char *)dst;
  size_t bytes_copied = 0;

  while (bytes_copied < len) {
    // 查找当前地址所在的VMA
    struct vm_area_struct *vma = mm_find_vma(mm, src_addr + bytes_copied);
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
    int page_idx = (page_va - vma->vm_start) / PAGE_SIZE;
    if (page_idx < 0 || page_idx >= vma->page_count)
      return bytes_copied > 0 ? bytes_copied : -1;

    // 确保页已分配
    if (!vma->pages[page_idx]) {
      // 页未分配，对于读操作这是错误
      return bytes_copied > 0 ? bytes_copied : -1;
    }

    // 获取页的物理地址
    void *pa = page_to_virt(vma->pages[page_idx]);
    if (!pa)
      return bytes_copied > 0 ? bytes_copied : -1;

    // 计算实际源地址（内核视角）
    const char *source = (const char *)pa + page_offset;

    // 复制数据
    memcpy(dst_ptr + bytes_copied, source, page_bytes);
    bytes_copied += page_bytes;
  }

  return bytes_copied;
}
