/**
 * @file pagetable.c
 * @brief RISC-V SV39 页表管理模块的实现
 */

#include <kernel/mm/page.h>
#include <kernel/mm/pagetable.h>

#include <spike_interface/spike_utils.h>

#include <util/spinlock.h>
#include <util/string.h>

// pointer to kernel page director
pagetable_t g_kernel_pagetable;
// 页表锁，保护全局页表操作
static spinlock_t pagetable_lock = SPINLOCK_INIT;
// 全局页表统计信息
pagetable_stats_t pt_stats;

#define VIRTUAL_TO_PHYSICAL(vaddr) ((uint64)(vaddr))
#define PHYSICAL_TO_VIRTUAL(paddr) ((uint64)(paddr))

// 全局页表模块初始化（同时记录内核页表+所有用户页表的元数据）
// 在kmem_init()中被调用
void pagetable_server_init(void) {
  // 初始化页表统计信息
  atomic_set(&pt_stats.mapped_pages, 0);
  atomic_set(&pt_stats.page_tables, 0);
}

/**
 * 创建一个新的空页表
 */
pagetable_t create_pagetable(void) {
  // 分配一个物理页作为根页表
  pagetable_t pagetable = (pagetable_t)alloc_page()->virtual_address;
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
      free_page((virt_to_page(next_pt)));
      // 释放下一级页表页
      atomic_dec(&pt_stats.page_tables);
    }
  }
}

/**
 * 释放整个页表结构
 */
void free_pagetable(pagetable_t pagetable) {
  if (pagetable == NULL) {
    return;
  }

  // 递归释放所有级别的页表
  _pagetable_free_level(pagetable, 0);

  // 释放根页表
  free_page((virt_to_page(pagetable)));

  atomic_dec(&pt_stats.page_tables);
}

/**
 * 在页表中查找页表项
 */
pte_t *pgt_walk(pagetable_t pagetable, uaddr va, int alloc) {
	if(va == 0x87ffd000){
		sprint("0x87ffd000\n");
	}
  if (pagetable == NULL) {
    return NULL;
  }

  // 检查虚拟地址是否有效
  if (va >= MAXVA) {

    return NULL;
  }

  // starting from the page directory
  pagetable_t pt = pagetable;

  for (int level = 2; level > 0; level--) {

    pte_t *pte = pt + PX(level, va);

		if((uint64)pte == 0x0000000087ffcfe8){
			sprint("L3 debug\n");
			
		}

    if (*pte & PTE_V) {

      pt = (pagetable_t)PTE2PA(*pte);
    } else {
			
      if (alloc && ((pt = (pte_t *)alloc_page()->virtual_address) != 0)) {
        memset(pt, 0, PAGE_SIZE);
        // writes the physical address of newly allocated page to pte, to
        // establish the page table tree.

        *pte = PA2PPN(pt) | PTE_V;
				if(*pte == 0x0000000021fff801){
					sprint("L1 debug\n");
				}else if(*pte == 0x0000000021fff001){
					sprint("L2 debug\n");
				}
      } else {
        sprint("pgt_walk: invalid pte! va = %lx\n", va);

        return 0;

      } // returns NULL, if alloc == 0, or no more physical page remains
    }
  }

  // return a PTE which contains phisical address of a page
  return pt + PX(0, va);
}

/**
 * 在页表中映射虚拟地址到物理地址(单页映射)
 * @param pagetable 页表指针
 * @param va 虚拟地址
 * @param pa 物理地址
 * @param perm 页权限标志
 * @return 成功返回0，失败返回-1
 */
int pgt_map_page(pagetable_t pagetable, uaddr va, uint64 pa, int perm) {
  if (pagetable == NULL) {
    return -1;
  }
  // sprint("debug\n");
  // 将地址对齐到页边界
  uaddr aligned_va = ROUNDDOWN(va, PAGE_SIZE);
  uint64 aligned_pa = ROUNDDOWN(pa, PAGE_SIZE);

  // 检查虚拟地址是否有效
  if (aligned_va >= MAXVA) {
    return -1;
  }

  // 锁定页表操作
  long flags = spinlock_lock_irqsave(&pagetable_lock);

  // 查找页表项，必要时分配页表
  pte_t *pte = pgt_walk(pagetable, aligned_va, 1);
  if (pte == NULL) {
    spinlock_unlock_irqrestore(&pagetable_lock, flags);
    return -1;
  }

  // 检查是否已映射
  if (*pte & PTE_V) {
    // 页已映射，可能需要更新权限
    if (PTE2PA(*pte) == aligned_pa) {
      // 同一物理页，只更新权限
      *pte = PA2PPN(aligned_pa) | perm | PTE_V;
    } else {
      // 映射到不同物理页，报错
      spinlock_unlock_irqrestore(&pagetable_lock, flags);
      return -1;
    }
  } else {
    // 创建新映射
    *pte = PA2PPN(aligned_pa) | perm | PTE_V;
    atomic_inc(&pt_stats.mapped_pages);
  }

  spinlock_unlock_irqrestore(&pagetable_lock, flags);
  return 0;
}

int pgt_map_pages(pagetable_t pagetable, uaddr va, uint64 pa, uint64 size,
                  int perm) {
  if (size < 0) {
    sprint("pgt_map_pages: wrong size %d\n", size);
    panic();
  }
  size = ROUNDUP(size, PAGE_SIZE);
  sprint("pgt_map_pages: start\n");
  for (uint64 off = 0; off < size; off+= PAGE_SIZE) {
    pgt_map_page(pagetable, va + off, pa + off, perm);
  }
  sprint("pgt_map_pages: complete\n");

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
        free_page((virt_to_page(pa)));
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
uint64 pgt_lookuppa(pagetable_t pagetable, uaddr va) {
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

void pagetable_activate(pagetable_t pagetable) {
  sprint("pagetable_activate: start.\n");
  pagetable_dump(pagetable);
  // 确保所有映射都已完成
  write_csr(satp, MAKE_SATP(pagetable));
  flush_tlb(); // 刷新TLB
  sprint("pagetable_activate: complete.\n");
}

pagetable_t pagetable_current(void) {
  uint64 satp = read_csr(satp);
  if ((satp >> 60) != SATP_MODE_SV39) {
    return NULL; // 不是SV39模式
  }
  return (pagetable_t)((satp & ((1L << 44) - 1)) << PAGE_SHIFT);
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
  pagetable_t dst = create_pagetable();
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
      void *new_page = alloc_page()->virtual_address;
      if (new_page == NULL) {
        // 内存不足，释放已分配内容并返回
        spinlock_unlock_irqrestore(&pagetable_lock, flags);
        free_pagetable(dst);
        return NULL;
      }

      // 复制页内容
      memcpy(new_page, (void *)pa, PAGE_SIZE);

      // 在新页表中创建映射
      pte_t *dst_pte = pgt_walk(dst, va, 1);
      if (dst_pte == NULL) {
        free_page((virt_to_page(new_page)));

        spinlock_unlock_irqrestore(&pagetable_lock, flags);
        free_pagetable(dst);
        return NULL;
      }

      *dst_pte = PA2PPN(new_page) | perm | PTE_V;
      atomic_inc(&pt_stats.mapped_pages);

    } else if (share == 1) {
      // 共享物理页: 直接映射到同一物理页
      pte_t *dst_pte = pgt_walk(dst, va, 1);
      if (dst_pte == NULL) {
        spinlock_unlock_irqrestore(&pagetable_lock, flags);
        free_pagetable(dst);
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
        free_pagetable(dst);
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
    sprint("  ");
  }
  sprint("Page table @0x%lx\n", (uint64)pagetable);

  // 遍历当前页表的所有条目
  for (int i = 0; i < PT_ENTRIES; i++) {
    pte_t pte = pagetable[i];
    if (pte & PTE_V) {
      // 计算此项对应的虚拟地址
      uaddr va =
          va_prefix | ((uaddr)i << (PAGE_SHIFT +
                                    PT_INDEX_BITS * (PAGE_LEVELS - 1 - level)));

      // 打印缩进
      for (int j = 0; j < level + 1; j++) {
        sprint("  ");
      }

      // 打印索引和虚拟地址
      sprint("[%d] VA 0x%lx -> PA 0x%lx, flags: ", i, va, PTE2PA(pte));

      // 打印权限标志
      if (pte & PTE_R)
        sprint("R");
      if (pte & PTE_W)
        sprint("W");
      if (pte & PTE_X)
        sprint("X");
      if (pte & PTE_U)
        sprint("U");
      if (pte & PTE_G)
        sprint("G");
      if (pte & PTE_A)
        sprint("A");
      if (pte & PTE_D)
        sprint("D");
      sprint("\n");

      // 如果不是叶子级别，则递归打印下一级
      if (level < PAGE_LEVELS - 1 && !(pte & (PTE_R | PTE_W | PTE_X))) {
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
    sprint("NULL page table\n");
    return;
  }

  sprint("=== Page Table Dump ===\n");
  _pagetable_dump_level(pagetable, 0, 0);

  // 打印统计信息
  sprint("Stats: %d page tables, %d mapped pages\n",
         atomic_read(&pt_stats.page_tables),
         atomic_read(&pt_stats.mapped_pages));
  sprint("=======================\n");
}

// 检查特定地址的映射
void check_address_mapping(pagetable_t pagetable, uint64 va) {
  uint64 vpn[3];
  vpn[2] = (va >> 30) & 0x1FF;
  vpn[1] = (va >> 21) & 0x1FF;
  vpn[0] = (va >> 12) & 0x1FF;

  sprint("Checking mapping for address 0x%lx (vpn: %d,%d,%d)\n", va, vpn[2],
         vpn[1], vpn[0]);

  // 检查第一级
  pte_t *pte1 = &pagetable[vpn[2]];
  sprint("L1 PTE at 0x%lx: 0x%lx\n", (uint64)pte1, *pte1);
  if (!(*pte1 & PTE_V)) {
    sprint("  Invalid L1 entry!\n");
    return;
  }

  // 检查第二级
  pagetable_t pt2 = (pagetable_t)PTE2PA(*pte1);
  // 转换为虚拟地址以便访问
  pt2 = (pagetable_t)PHYSICAL_TO_VIRTUAL((uint64)pt2);
  pte_t *pte2 = &pt2[vpn[1]];
  sprint("L2 PTE at 0x%lx: 0x%lx\n", (uint64)pte2, *pte2);
  if (!(*pte2 & PTE_V)) {
    sprint("  Invalid L2 entry!\n");
    return;
  }

  // 检查第三级
  pagetable_t pt3 = (pagetable_t)PTE2PA(*pte2);
  pt3 = (pagetable_t)PHYSICAL_TO_VIRTUAL((uint64)pt3);
  pte_t *pte3 = &pt3[vpn[0]];
  sprint("L3 PTE at 0x%lx: 0x%lx\n", (uint64)pte3, *pte3);
  if (!(*pte3 & PTE_V)) {
    sprint("  Invalid L3 entry!\n");
    return;
  }

  // 分析最终PTE的权限
  sprint("  Physical addr: 0x%lx\n", PTE2PA(*pte3));
  sprint("  Permissions: %s%s%s%s%s%s%s\n", (*pte3 & PTE_R) ? "R" : "-",
         (*pte3 & PTE_W) ? "W" : "-", (*pte3 & PTE_X) ? "X" : "-",
         (*pte3 & PTE_U) ? "U" : "-", (*pte3 & PTE_G) ? "G" : "-",
         (*pte3 & PTE_A) ? "A" : "-", (*pte3 & PTE_D) ? "D" : "-");
}
