#include <kernel/mmap.h>
#include <kernel/mm_struct.h>
#include <kernel/pagetable.h>

/**
 * 将单个物理页映射到虚拟地址
 * @param mm        内存描述符
 * @param vaddr     虚拟地址
 * @param page      物理页结构
 * @param prot      保护标志
 * @return          成功返回0，失败返回错误码
 */
int map_page(struct mm_struct *mm, unsigned long vaddr, struct page *page,
             pgprot_t prot) {
  // 确保地址页对齐
  if (vaddr & (PAGE_SIZE - 1))
    return -EINVAL;

  // 获取页框号
  unsigned long pfn = page_to_pfn(page);

  // 建立映射
  pte_t *pte = page_walk(mm->pagetable, vaddr, 1);
  if (!pte)
    return -ENOMEM;

  if (*pte & _PAGE_PRESENT)
    return -EEXIST; // 已存在映射

  *pte = pfn_pte(pfn, prot);
  return 0;
}

/**
 * 取消单个虚拟地址的映射
 * @param mm        内存描述符
 * @param vaddr     虚拟地址
 * @return          成功返回0，失败返回错误码
 */
int unmap_page(struct mm_struct *mm, unsigned long vaddr) {
  // 确保地址页对齐
  if (vaddr & (PAGE_SIZE - 1))
    return -EINVAL;

  pte_t *pte = page_walk(mm->pagetable, vaddr, 0);
  if (!pte || !(*pte & _PAGE_PRESENT))
    return -EFAULT; // 映射不存在

  // 清除页表项
  *pte = 0;
  return 0;
}

/**
 * 修改单个页面映射的保护属性
 * @param mm        内存描述符
 * @param vaddr     虚拟地址
 * @param prot      新的保护标志
 * @return          成功返回0，失败返回错误码
 */
int protect_page(struct mm_struct *mm, unsigned long vaddr, pgprot_t prot) {
	// 确保地址页对齐
	if (vaddr & (PAGE_SIZE-1))
			return -EINVAL;
	
	pte_t *pte = page_walk(mm->pagetable, vaddr, 0);
	if (!pte || !(*pte & _PAGE_PRESENT))
			return -EFAULT; // 映射不存在
	
	// 保留物理页号，修改保护位
	unsigned long pfn = pte_pfn(*pte);
	*pte = pfn_pte(pfn, prot);
	
	return 0;
}




/**
 * 查询虚拟地址的物理映射
 * @param mm        内存描述符
 * @param addr      虚拟地址
 * @param pfn       返回的物理页框号
 * @param prot      返回的保护标志
 * @return          成功返回0，失败返回错误码
 */
int query_vm_mapping(struct mm_struct *mm, unsigned long addr,
                     unsigned long *pfn, pgprot_t *prot) {
  pte_t *pte = page_walk(mm->pagetable, addr, 0);
  if (!pte || !(*pte & _PAGE_PRESENT))
    return -EFAULT; // 映射不存在

  if (pfn)
    *pfn = pte_pfn(*pte);
  if (prot)
    *prot = pte_pgprot(*pte);

  return 0;
}

/**
 * 将保护标志转换为页表项标志
 */
static inline pgprot_t vm_get_page_prot(unsigned long vm_flags) {
  pgprot_t prot = __pgprot(0);

  if (vm_flags & VM_READ)
    prot = __pgprot(prot | _PAGE_READ);
  if (vm_flags & VM_WRITE)
    prot = __pgprot(prot | _PAGE_WRITE);
  if (vm_flags & VM_EXEC)
    prot = __pgprot(prot | _PAGE_EXEC);
  if (vm_flags & VM_USER)
    prot = __pgprot(prot | _PAGE_USER);

  return prot;
}

/**
 * 从页表项中提取物理页框号
 */
static inline unsigned long pte_pfn(pte_t pte) {
  return (pte_val(pte) & PTE_PFN_MASK) >> PAGE_SHIFT;
}

/**
 * 从物理页框号和保护标志构建页表项
 */
static inline pte_t pfn_pte(unsigned long pfn, pgprot_t prot) {
  return __pte((pfn << PAGE_SHIFT) | pgprot_val(prot));
}


//
// traverse the page table (starting from page_dir) to find the corresponding
// pte of va. returns: PTE (page table entry) pointing to va.
// 实际查询va对应的页表项的过程，可以选择是否在查询过程中为页中间目录和页表分配内存空间。
// 拿一个物理页当页表并不影响在页表中初始化这个物理页的映射关系。
pte_t *page_walk(pagetable_t page_dir, uint64 va, int alloc) {
  if (va >= MAXVA)
    panic("page_walk");

  // starting from the page directory
  pagetable_t pt = page_dir;

  // traverse from page directory to page table.
  // as we use risc-v sv39 paging scheme, there will be 3 layers: page dir,
  // page medium dir, and page table.
  for (int level = 2; level > 0; level--) {
    // macro "PX" gets the PTE index in page table of current_percpu level
    // "pte" points to the entry of current level
    pte_t *pte = pt + PX(level, va);

    // now, we need to know if above pte is valid (established mapping to a
    // phyiscal page) or not.
    if (*pte & PTE_V) { // PTE valid
      // phisical address of pagetable of next level
      pt = (pagetable_t)PTE2PA(*pte);
    } else { // PTE invalid (not exist).
      // allocate a page (to be the new pagetable), if alloc == 1
      if (alloc && ((pt = (pte_t *)alloc_page(1)) != 0)) {
        memset(pt, 0, PGSIZE);
        // writes the physical address of newly allocated page to pte, to
        // establish the page table tree.
        *pte = PA2PPN(pt) | PTE_V;
      } else // returns NULL, if alloc == 0, or no more physical page remains
        return 0;
    }
  }

  // return a PTE which contains phisical address of a page
  return pt + PX(0, va);
}