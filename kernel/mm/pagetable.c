/**
 * @file pagetable.c
 * @brief RISC-V SV39 页表管理模块的实现
 */

#include <kernel/page.h>
#include <kernel/pagetable.h>
#include <kernel/pmm.h>
#include <kernel/spinlock.h>
#include <kernel/vmm.h>
#include <util/string.h>

// pointer to kernel page director
pagetable_t g_kernel_pagetable;
// 页表锁，保护全局页表操作
static spinlock_t pagetable_lock = SPINLOCK_INIT;
// 全局页表统计信息
pagetable_stats_t pt_stats;

/**
 * 初始化页表子系统
 */
void pagetable_init(void) {
  // 初始化页表统计信息
  atomic_set(&pt_stats.mapped_pages, 0);
  atomic_set(&pt_stats.page_tables, 0);
}

/**
 * 创建一个新的空页表
 */
pagetable_t pagetable_create(void) {
  // 分配一个物理页作为根页表
  pagetable_t pagetable = (pagetable_t)Alloc_page();
  if (pagetable == NULL) {
    return NULL;
  }

  // 清零页表
  memset(pagetable, 0, PAGE_SIZE);

  // 更新页表统计信息
  atomic_inc(&pt_stats.page_tables);

  return pagetable;
}

/**
 * 递归释放页表页
 * @param pagetable 要释放的页表
 * @param level 当前页表级别(0=根页表, 1=中间页表, 2=叶子页表)
 */
static void _pagetable_free_level(pagetable_t pagetable, int level) {
  // 叶子页表直接返回，不需要递归
  if (level == PAGE_LEVELS - 1) {
    return;
  }

  // 遍历当前页表的所有条目
  for (int i = 0; i < PT_ENTRIES; i++) {
    pte_t pte = pagetable[i];
    // 如果页表项有效，则递归释放下一级页表
    if (pte & PTE_V) {
      pagetable_t next_pt = (pagetable_t)PTE2PA(pte);
      _pagetable_free_level(next_pt, level + 1);

      // 释放下一级页表页
      free_page(next_pt);
      atomic_dec(&pt_stats.page_tables);
    }
  }
}

/**
 * 释放整个页表结构
 */
void pagetable_free(pagetable_t pagetable) {
  if (pagetable == NULL) {
    return;
  }

  // 递归释放所有级别的页表
  _pagetable_free_level(pagetable, 0);

  // 释放根页表
  free_page(pagetable);
  atomic_dec(&pt_stats.page_tables);
}

/**
 * 在页表中查找页表项
 */
pte_t *pgt_walk(pagetable_t pagetable, uaddr va, int alloc) {
  if (pagetable == NULL) {
    return NULL;
  }

  // 检查虚拟地址是否有效
  if (va >= MAXVA) {
    return NULL;
  }

  // 遍历三级页表
  for (int level = 0; level < PAGE_LEVELS - 1; level++) {
    // 计算当前级别的页表索引
    int index =
        (va >> (PAGE_SHIFT + PT_INDEX_BITS * (PAGE_LEVELS - 1 - level))) &
        PT_INDEX_MASK;
    pte_t *pte = &pagetable[index];

    // 如果页表项有效，则继续到下一级
    if (*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      // 页表不存在
      if (!alloc) {
        return NULL; // 不分配
      }

      // 分配新的页表页
      pagetable_t new_pt = (pagetable_t)Alloc_page();
      if (new_pt == NULL) {
        return NULL; // 内存不足
      }

      // 初始化新页表
      memset(new_pt, 0, PAGE_SIZE);
      atomic_inc(&pt_stats.page_tables);

      // 更新当前页表项指向新页表
      *pte = PA2PPN(new_pt) | PTE_V;
      pagetable = new_pt;
    }
  }

  // 计算最后一级页表的索引
  int index = (va >> PAGE_SHIFT) & PT_INDEX_MASK;
  return &pagetable[index];
}

/**
 * 在页表中映射虚拟地址到物理地址
 */
int pgt_map(pagetable_t pagetable, uaddr va, uint64 pa, uint64 size, int perm) {
  if (pagetable == NULL) {
    return -1;
  }

  // 确保地址对齐到页边界
  uaddr start_va = ROUNDDOWN(va, PAGE_SIZE);
  uaddr end_va = ROUNDUP(va + size, PAGE_SIZE);
  uint64 start_pa = ROUNDDOWN(pa, PAGE_SIZE);

  // 检查虚拟地址范围是否有效
  if (start_va >= MAXVA || end_va > MAXVA || end_va < start_va) {
    return -1;
  }

  // 锁定页表操作
  long flags = spinlock_lock_irqsave(&pagetable_lock);

  // 逐页映射
  for (uaddr va_page = start_va, pa_page = start_pa; va_page < end_va;
       va_page += PAGE_SIZE, pa_page += PAGE_SIZE) {

    // 查找页表项，必要时分配页表
    pte_t *pte = pgt_walk(pagetable, va_page, 1);
    if (pte == NULL) {
      // 分配页表失败，回滚已映射的页
      pgt_unmap(pagetable, start_va, va_page - start_va, 0);
      spinlock_unlock_irqrestore(&pagetable_lock, flags);
      return -1;
    }

    // 检查是否已映射
    if (*pte & PTE_V) {
      // 页已映射，可能需要更新权限
      if (PTE2PA(*pte) == pa_page) {
        // 同一物理页，只更新权限
        *pte = PA2PPN(pa_page) | perm | PTE_V;
      } else {
        // 映射到不同物理页，报错
        spinlock_unlock_irqrestore(&pagetable_lock, flags);
        return -1;
      }
    } else {
      // 创建新映射
      *pte = PA2PPN(pa_page) | perm | PTE_V;
      atomic_inc(&pt_stats.mapped_pages);
    }
  }

  spinlock_unlock_irqrestore(&pagetable_lock, flags);
  return 0;
}

/**
 * 解除页表中一块虚拟地址区域的映射
 */
int pgt_unmap(pagetable_t pagetable, uaddr va, uint64 size, int free_phys) {
  if (pagetable == NULL) {
    return -1;
  }

  // 确保地址对齐到页边界
  uaddr start_va = ROUNDDOWN(va, PAGE_SIZE);
  uaddr end_va = ROUNDUP(va + size, PAGE_SIZE);

  // 检查虚拟地址范围是否有效
  if (start_va >= MAXVA || end_va > MAXVA || end_va < start_va) {
    return -1;
  }

  // 锁定页表操作
  long flags = spinlock_lock_irqsave(&pagetable_lock);

  // 逐页取消映射
  for (uaddr va_page = start_va; va_page < end_va; va_page += PAGE_SIZE) {
    // 查找页表项，不分配新页表
    pte_t *pte = pgt_walk(pagetable, va_page, 0);
    if (pte == NULL) {
      // 页表不存在，跳过
      continue;
    }

    // 检查页是否已映射
    if (*pte & PTE_V) {
      // 如果需要，释放物理页
      if (free_phys) {
        void *pa = (void *)PTE2PA(*pte);
        free_page(pa);
      }

      // 清除页表项
      *pte = 0;
      atomic_dec(&pt_stats.mapped_pages);
    }
  }

  spinlock_unlock_irqrestore(&pagetable_lock, flags);

  // 刷新TLB
  flush_tlb();

  return 0;
}

/**
 * 查找虚拟地址对应的物理地址
 */
void *pgt_lookuppa(pagetable_t pagetable, uaddr va) {
  // 查找页表项
  pte_t *pte = pgt_walk(pagetable, va, 0);
  if (pte == NULL || !(*pte & PTE_V)) {
    return 0; // 映射不存在
  }

  // 计算页内偏移
  uint64 offset = va & (PAGE_SIZE - 1);

  // 返回物理地址
  return PTE2PA(*pte) | offset;
}

/**
 * 复制页表结构
 *
 * @param share 映射类型:
 *   0: 复制物理页(每个映射的地址都有一个新的物理页副本)
 *   1: 共享物理页(仅复制页表结构，物理页共享)
 *   2: 写时复制(COW，复制页表结构并将源和目标页表项标记为只读)
 */
pagetable_t pagetable_copy(pagetable_t src, uaddr start, uaddr end, int share) {
  if (src == NULL) {
    return NULL;
  }

  // 确保地址对齐到页边界
  start = ROUNDDOWN(start, PAGE_SIZE);
  end = ROUNDUP(end, PAGE_SIZE);

  // 创建新的页表
  pagetable_t dst = pagetable_create();
  if (dst == NULL) {
    return NULL;
  }

  // 锁定页表操作
  long flags = spinlock_lock_irqsave(&pagetable_lock);

  // 逐页复制映射
  for (uaddr va = start; va < end; va += PAGE_SIZE) {
    // 查找源页表项
    pte_t *src_pte = pgt_walk(src, va, 0);
    if (src_pte == NULL || !(*src_pte & PTE_V)) {
      // 源页表中没有映射，跳过
      continue;
    }

    // 获取源物理地址和权限
    uint64 pa = PTE2PA(*src_pte);
    int perm = PTE_FLAGS(*src_pte);

    // 根据共享模式处理
    if (share == 0) {
      // 完全复制: 分配新物理页并复制内容
      void *new_page = Alloc_page();
      if (new_page == NULL) {
        // 内存不足，释放已分配内容并返回
        spinlock_unlock_irqrestore(&pagetable_lock, flags);
        pagetable_free(dst);
        return NULL;
      }

      // 复制页内容
      memcpy(new_page, (void *)pa, PAGE_SIZE);

      // 在新页表中创建映射
      pte_t *dst_pte = pgt_walk(dst, va, 1);
      if (dst_pte == NULL) {
        free_page(new_page);
        spinlock_unlock_irqrestore(&pagetable_lock, flags);
        pagetable_free(dst);
        return NULL;
      }

      *dst_pte = PA2PPN(new_page) | perm | PTE_V;
      atomic_inc(&pt_stats.mapped_pages);

    } else if (share == 1) {
      // 共享物理页: 直接映射到同一物理页
      pte_t *dst_pte = pgt_walk(dst, va, 1);
      if (dst_pte == NULL) {
        spinlock_unlock_irqrestore(&pagetable_lock, flags);
        pagetable_free(dst);
        return NULL;
      }

      *dst_pte = PA2PPN(pa) | perm | PTE_V;
      atomic_inc(&pt_stats.mapped_pages);

    } else if (share == 2) {
      // 写时复制: 共享物理页，但移除写权限

      // 在新页表中创建映射，但标记为只读
      pte_t *dst_pte = pgt_walk(dst, va, 1);
      if (dst_pte == NULL) {
        spinlock_unlock_irqrestore(&pagetable_lock, flags);
        pagetable_free(dst);
        return NULL;
      }

      // 设置映射，但确保是只读的
      *dst_pte = PA2PPN(pa) | (perm & ~PTE_W) | PTE_V;
      atomic_inc(&pt_stats.mapped_pages);

      // 如果源页表项有写权限，也需要移除以实现COW
      if (perm & PTE_W) {
        *src_pte = PA2PPN(pa) | (perm & ~PTE_W) | PTE_V;
      }
    }
  }

  spinlock_unlock_irqrestore(&pagetable_lock, flags);
  return dst;
}

/**
 * 打印页表内容(递归方式)
 * @param pagetable 要打印的页表
 * @param level 当前页表级别
 * @param va_prefix 虚拟地址前缀
 */
static void _pagetable_dump_level(pagetable_t pagetable, int level,
                                  uaddr va_prefix) {
  // 打印缩进
  for (int i = 0; i < level; i++) {
    putstring("  ");
  }
  putstring("Page table @0x");

  // 打印页表物理地址(十六进制)
  char addr_buf[20];
  sprint(addr_buf, "%lx", (uint64)pagetable);
  putstring(addr_buf);
  putstring("\n");

  // 遍历当前页表的有效条目
  for (int i = 0; i < PT_ENTRIES; i++) {
    pte_t pte = pagetable[i];
    if (pte & PTE_V) {
      // 计算此项对应的虚拟地址
      uaddr va =
          va_prefix | ((uaddr)i << (PAGE_SHIFT +
                                    PT_INDEX_BITS * (PAGE_LEVELS - 1 - level)));

      // 打印缩进
      for (int j = 0; j < level + 1; j++) {
        putstring("  ");
      }

      // 打印索引和虚拟地址
      char entry_buf[100];
      sprint(entry_buf, "[%d] VA 0x%lx -> PA 0x%lx, flags: ", i, va,
             PTE2PA(pte));
      putstring(entry_buf);

      // 打印权限标志
      if (pte & PTE_R)
        putstring("R");
      if (pte & PTE_W)
        putstring("W");
      if (pte & PTE_X)
        putstring("X");
      if (pte & PTE_U)
        putstring("U");
      if (pte & PTE_G)
        putstring("G");
      if (pte & PTE_A)
        putstring("A");
      if (pte & PTE_D)
        putstring("D");
      putstring("\n");

      // 如果不是叶子级别，则递归打印下一级
      if (level < PAGE_LEVELS - 1) {
        _pagetable_dump_level((pagetable_t)PTE2PA(pte), level + 1, va);
      }
    }
  }
}

/**
 * 打印页表内容(用于调试)
 */
void pagetable_dump(pagetable_t pagetable) {
  if (pagetable == NULL) {
    putstring("NULL page table\n");
    return;
  }

  putstring("=== Page Table Dump ===\n");
  _pagetable_dump_level(pagetable, 0, 0);

  // 打印统计信息
  char stats_buf[100];
  sprint(stats_buf, "Stats: %d page tables, %d mapped pages\n",
         atomic_read(&pt_stats.page_tables),
         atomic_read(&pt_stats.mapped_pages));
  putstring(stats_buf);
  putstring("=======================\n");
}