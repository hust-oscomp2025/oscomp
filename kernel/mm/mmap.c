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