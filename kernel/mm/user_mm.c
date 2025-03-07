#include "spike_interface/spike_utils.h"
#include <kernel/atomic.h>
#include <kernel/memlayout.h>
#include <kernel/page.h>
#include <kernel/pmm.h>
#include <kernel/process.h>
#include <kernel/user_mem.h>
#include <kernel/vmm.h>
#include <util/string.h>

/**
 * 初始化用户内存管理子系统
 */
void user_mem_init(void) {
  // 初始化可能的全局资源
  sprint("User memory management subsystem initialized\n");
}

/**
 * 创建用户内存布局
 */
struct mm_struct *user_mm_create(void) {
  struct mm_struct *mm = (struct mm_struct *)kmalloc(sizeof(struct mm_struct));
  if (!mm)
    return NULL;

  // 初始化mm结构
  memset(mm, 0, sizeof(struct mm_struct));
  INIT_LIST_HEAD(&mm->mmap);
  mm->map_count = 0;
  atomic_flag_clear(&mm->mm_lock.lock);
  atomic_set(&mm->mm_users, 1);
  atomic_set(&mm->mm_count, 1);

  // 用户空间默认布局
  mm->start_code = 0x00400000;               // 代码段默认起始地址
  mm->end_code = 0x00400000;                 // 初始时代码段为空
  mm->start_data = 0x10000000;               // 数据段默认起始地址
  mm->end_data = 0x10000000;                 // 初始时数据段为空
  mm->start_brk = USER_FREE_ADDRESS_START;   // 堆默认起始地址
  mm->brk = USER_FREE_ADDRESS_START;         // 初始时堆为空
  mm->start_stack = USER_STACK_TOP - PGSIZE; // 栈默认起始地址
  mm->end_stack = USER_STACK_TOP;            // 栈默认结束地址

  return mm;
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
    list_for_each_entry_safe(vma, tmp, &mm->mmap, vm_list) {
      // 释放VMA关联的所有页
      if (vma->pages) {
        for (int i = 0; i < vma->page_count; i++) {
          if (vma->pages[i]) {
            page_free(vma->pages[i]);
          }
        }
        kfree(vma->pages);
      }

      // 从链表中移除并释放VMA
      list_del(&vma->vm_list);
      kfree(vma);
    }

    // 释放mm结构
    kfree(mm);
  }
}

/**
 * 为进程初始化内存布局
 */
int setup_user_memory(process *proc) {
  if (!proc)
    return -1;

  // 创建mm结构
  struct mm_struct *mm = user_mm_create();
  if (!mm)
    return -1;

  // 关联到进程
  proc->mm = mm;
  mm->pgd = proc->pagetable;

  // 初始化栈区域
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
      user_alloc_page(proc, mm->start_stack, PROT_READ | PROT_WRITE);
  if (!page_addr) {
    user_mm_free(mm);
    proc->mm = NULL;
    return -1;
  }

  return 0;
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

  atomic_flag_clear(&vma->vma_lock.lock);

  // 计算需要的页数
  vma->page_count = (end - start + PGSIZE - 1) / PGSIZE;

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
  list_add(&vma->vm_list, &mm->mmap);
  mm->map_count++;

  return vma;
}

/**
 * 查找包含指定地址的VMA
 */
struct vm_area_struct *find_vma(struct mm_struct *mm, uint64 addr) {
  if (!mm)
    return NULL;

  struct vm_area_struct *vma;
  list_for_each_entry(vma, &mm->mmap, vm_list) {
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
  list_for_each_entry(vma, &mm->mmap, vm_list) {
    // 检查是否有重叠
    if (start < vma->vm_end && end > vma->vm_start)
      return vma;
  }

  return NULL;
}

/**
 * 映射内存区域
 */
uint64 do_mmap(process *proc, uint64 addr, size_t length, int prot,
               enum vma_type type, uint64 flags) {
  if (!proc || !proc->mm || length == 0)
    return -1;

  struct mm_struct *mm = proc->mm;

  // 对齐长度到页大小
  length = ROUNDUP(length, PGSIZE);

  // 如果未指定地址，则自动分配
  if (addr == 0) {
    // 简单策略：从堆区域之后查找空闲区域
    addr = mm->brk;

    // 检查是否与现有区域重叠
    while (find_vma_intersection(mm, addr, addr + length)) {
      addr += PGSIZE;
    }
  } else {
    // 检查指定地址是否可用
    if (find_vma_intersection(mm, addr, addr + length))
      return -1;
  }

  // 创建VMA
  struct vm_area_struct *vma =
      create_vma(mm, addr, addr + length, prot, type, flags);
  if (!vma)
    return -1;

  // 对于匿名映射，按需分配物理页（延迟分配）
  // 实际页分配在页错误处理时进行

  return addr;
}

/**
 * 取消映射内存区域
 */
int do_munmap(process *proc, uint64 addr, size_t length) {
  if (!proc || !proc->mm || length == 0)
    return -1;

  struct mm_struct *mm = proc->mm;

  // 对齐到页大小
  addr = ROUNDDOWN(addr, PGSIZE);
  length = ROUNDUP(length, PGSIZE);
  uint64 end = addr + length;

  // 查找并处理所有受影响的VMA
  struct vm_area_struct *vma, *tmp;
  list_for_each_entry_safe(vma, tmp, &mm->mmap, vm_list) {
    // 检查是否有重叠
    if (addr < vma->vm_end && end > vma->vm_start) {
      // 计算重叠区域
      __user uint64 overlap_start = MAX(addr, vma->vm_start);
      __user uint64 overlap_end = MIN(end, vma->vm_end);

      // 计算页索引
      int start_idx = (overlap_start - vma->vm_start) / PGSIZE;
      int end_idx = (overlap_end - vma->vm_start) / PGSIZE;

      // 释放和取消映射重叠区域的页
      for (int i = start_idx; i < end_idx; i++) {
        if (vma->pages[i]) {
          // 取消对应虚拟地址的映射
          uint64 page_va = vma->vm_start + (i * PGSIZE);
          user_vm_unmap(proc->pagetable, page_va, PGSIZE, 1);

          // 释放页
          page_free(vma->pages[i]);
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
        int new_start_idx = (new_start - vma->vm_start) / PGSIZE;
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
void *user_alloc_page(process *proc, __user uint64 addr, int prot) {
  if (!proc || !proc->pagetable)
    return NULL;

  // 分配一个页结构体
  struct page *page = page_alloc();
  if (!page)
    return NULL;

  // 获取页结构对应的物理地址
  void *pa = page_to_virt(page);

  // 映射到用户虚拟地址空间
  int result = map_pages(proc->pagetable, __user addr, PGSIZE, (uint64)pa,
                         prot_to_type(prot, 1));

  if (result != 0) {
    // 映射失败，释放页面
    page_free(page);
    return NULL;
  }

  // 记录映射关系
  if (proc->mm) {
    struct vm_area_struct *vma = find_vma(proc->mm, __user addr);
    if (vma) {
      int page_idx = (__user addr - vma->vm_start) / PGSIZE;
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
uint64 do_brk(process *proc, int64 increment) {
  if (!proc || !proc->mm)
    return -1;

  struct mm_struct *mm = proc->mm;

  if (increment == 0)
    return mm->brk; // 仅查询当前brk

  uint64 new_brk = mm->brk + increment;

  // 验证新brk是否有效
  if (new_brk < mm->start_brk)
    return -1; // 不能低于起始堆地址

  // 检查堆是否与其他区域重叠
  struct vm_area_struct *vma = find_vma(mm, new_brk - 1);
  if (vma && vma->vm_start < new_brk && vma->vm_type != VMA_HEAP)
    return -1; // 与非堆区域重叠

  if (increment > 0) {
    // 扩展堆
    vma = find_vma(mm, mm->brk);
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
      int new_page_count = (new_brk - vma->vm_start + PGSIZE - 1) / PGSIZE;
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
    vma = find_vma(mm, mm->brk - 1);
    if (vma && vma->vm_type == VMA_HEAP) {
      // 释放不再使用的页
      int start_idx = (new_brk - vma->vm_start + PGSIZE - 1) / PGSIZE;
      int end_idx = (old_brk - vma->vm_start + PGSIZE - 1) / PGSIZE;

      for (int i = start_idx; i < end_idx; i++) {
        if (vma->pages[i]) {
          uint64 page_va = vma->vm_start + (i * PGSIZE);
          user_vm_unmap(proc->pagetable, page_va, PGSIZE, 1);
          page_free(vma->pages[i]);
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
void *user_alloc_pages(process *proc, int nr_pages, uint64 addr, int prot) {
  if (!proc || !proc->mm || nr_pages <= 0)
    return NULL;

  // 计算所需空间大小
  size_t length = nr_pages * PGSIZE;

  // 分配虚拟地址空间
  uint64 mmap_addr =
      do_mmap(proc, addr, length, prot, VMA_ANONYMOUS, VM_PRIVATE);
  if (mmap_addr == (uint64)-1)
    return NULL;

  // 预分配所有页并立即映射（不使用按需分页）
  for (int i = 0; i < nr_pages; i++) {
    void *page_addr = user_alloc_page(proc, mmap_addr + (i * PGSIZE), prot);
    if (!page_addr) {
      // 分配失败，释放已分配的页
      do_munmap(proc, mmap_addr, i * PGSIZE);
      return NULL;
    }
  }

  return (void *)mmap_addr;
}

/**
 * 从用户空间释放页
 */
int user_free_pages(process *proc, uint64 addr, int nr_pages) {
  if (!proc || !proc->mm || nr_pages <= 0)
    return -1;

  // 计算释放区域大小
  size_t length = nr_pages * PGSIZE;

  // 取消映射并释放页
  return do_munmap(proc, addr, length);
}

/**
 * 安全地将数据复制到用户空间
 */
ssize_t copy_to_user(process *proc, void *dst, const void *src, size_t len) {
  if (!proc || !proc->mm || !dst || !src || len == 0)
    return -1;

  uint64 dst_addr = (uint64)dst;
  const char *src_ptr = (const char *)src;
  size_t bytes_copied = 0;

  while (bytes_copied < len) {
    // 查找当前地址所在的VMA
    struct vm_area_struct *vma = find_vma(proc->mm, dst_addr + bytes_copied);
    if (!vma)
      return bytes_copied > 0 ? bytes_copied : -1;

    // 检查写权限
    if (!(vma->vm_prot & PROT_WRITE))
      return bytes_copied > 0 ? bytes_copied : -1;

    // 计算当前页内的复制长度
    uint64 page_offset = (dst_addr + bytes_copied) % PGSIZE;
    uint64 page_bytes = MIN(PGSIZE - page_offset, len - bytes_copied);

    // 获取目标页的虚拟地址
    uint64 page_va = ROUNDDOWN(dst_addr + bytes_copied, PGSIZE);

    // 查找对应的页结构
    int page_idx = (page_va - vma->vm_start) / PGSIZE;
    if (page_idx < 0 || page_idx >= vma->page_count)
      return bytes_copied > 0 ? bytes_copied : -1;

    // 确保页已分配
    if (!vma->pages[page_idx]) {
      void *page_addr = user_alloc_page(proc, page_va, vma->vm_prot);
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
ssize_t copy_from_user(process *proc, void *dst, const void *src, size_t len) {
  if (!proc || !proc->mm || !dst || !src || len == 0)
    return -1;

  uint64 src_addr = (uint64)src;
  char *dst_ptr = (char *)dst;
  size_t bytes_copied = 0;

  while (bytes_copied < len) {
    // 查找当前地址所在的VMA
    struct vm_area_struct *vma = find_vma(proc->mm, src_addr + bytes_copied);
    if (!vma)
      return bytes_copied > 0 ? bytes_copied : -1;

    // 检查读权限
    if (!(vma->vm_prot & PROT_READ))
      return bytes_copied > 0 ? bytes_copied : -1;

    // 计算当前页内的复制长度
    uint64 page_offset = (src_addr + bytes_copied) % PGSIZE;
    uint64 page_bytes = MIN(PGSIZE - page_offset, len - bytes_copied);

    // 获取源页的虚拟地址
    uint64 page_va = ROUNDDOWN(src_addr + bytes_copied, PGSIZE);

    // 查找对应的页结构
    int page_idx = (page_va - vma->vm_start) / PGSIZE;
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

/**
 * 用户空间内存初始化
 * 为用户程序设置基本的内存布局
 */
int init_user_memory(process *proc) {
  if (!proc)
    return -1;

  // 初始化内存布局
  if (setup_user_memory(proc) != 0)
    return -1;

  // 为用户栈分配初始页
  void *stack_addr =
      user_alloc_page(proc, proc->mm->start_stack, PROT_READ | PROT_WRITE);
  if (!stack_addr)
    return -1;

  // 初始化堆
  proc->mm->start_brk = USER_FREE_ADDRESS_START;
  proc->mm->brk = USER_FREE_ADDRESS_START;

  return 0;
}

// 一些辅助函数，可根据需要添加

/**
 * 打印进程的内存布局信息
 */
void print_proc_memory_layout(process *proc) {
  if (!proc || !proc->mm)
    return;

  struct mm_struct *mm = proc->mm;

  sprint("Process %d memory layout:\n", proc->pid);
  sprint("  code: 0x%lx - 0x%lx\n", mm->start_code, mm->end_code);
  sprint("  data: 0x%lx - 0x%lx\n", mm->start_data, mm->end_data);
  sprint("  heap: 0x%lx - 0x%lx\n", mm->start_brk, mm->brk);
  sprint("  stack: 0x%lx - 0x%lx\n", mm->start_stack, mm->end_stack);

  sprint("  VMAs (%d):\n", mm->map_count);

  struct vm_area_struct *vma;
  list_for_each_entry(vma, &mm->mmap, vm_list) {
    const char *type_str;
    switch (vma->vm_type) {
    case VMA_ANONYMOUS:
      type_str = "anon";
      break;
    case VMA_FILE:
      type_str = "file";
      break;
    case VMA_STACK:
      type_str = "stack";
      break;
    case VMA_HEAP:
      type_str = "heap";
      break;
    case VMA_CODE:
      type_str = "code";
      break;
    case VMA_DATA:
      type_str = "data";
      break;
    default:
      type_str = "unknown";
      break;
    }

    char prot_str[8] = {0};
    if (vma->vm_prot & PROT_READ)
      strcat(prot_str, "r");
    if (vma->vm_prot & PROT_WRITE)
      strcat(prot_str, "w");
    if (vma->vm_prot & PROT_EXEC)
      strcat(prot_str, "x");

    sprint("    %s: 0x%lx - 0x%lx [%s] pages:%d\n", type_str, vma->vm_start,
           vma->vm_end, prot_str, vma->page_count);
  }
}