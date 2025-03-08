/*
 * virtual address mapping related functions.
 */

#include <kernel/mm_struct.h>
#include <kernel/vmm.h>

#include <spike_interface/spike_utils.h>

#include <kernel/memlayout.h>
#include <kernel/pmm.h>
#include <kernel/process.h>
#include <kernel/riscv.h>
#include <kernel/types.h>
#include <util/string.h>

/* --- utility functions for virtual address mapping --- */
//
// establish mapping of virtual address [va, va+size] to phyiscal address [pa,
// pa+size] with the permission of "perm".
//
//
int map_pages(pagetable_t page_dir, uint64 va, uint64 size, uint64 pa,
              int perm) {

  for (uint64 first = ROUNDDOWN(va, PGSIZE),
              last = ROUNDDOWN(va + size - 1, PGSIZE);
       first <= last; first += PGSIZE, pa += PGSIZE) {
    pte_t *pte;
    // sprint("first=0x%lx\n",first);
    if ((pte = page_walk(page_dir, first, 1)) == 0)
      return -1;
    // sprint("first=0x%lx\n",first);
    if (*pte & PTE_V) {
      // sprint("first=0x%lx\n",first);
      panic("map_pages fails on mapping va (0x%lx) to pa (0x%lx)", first, pa);
    }

    *pte = PA2PTE(pa) | perm | PTE_V;
  }
  return 0;
}

//
// convert permission code to permission types of PTE
//
uint64 prot_to_type(int prot, int user) {
  uint64 perm = 0;
  if (prot & PROT_READ)
    perm |= PTE_R | PTE_A;
  if (prot & PROT_WRITE)
    perm |= PTE_W | PTE_D;
  if (prot & PROT_EXEC)
    perm |= PTE_X | PTE_A;
  if (perm == 0)
    perm = PTE_R;
  if (user)
    perm |= PTE_U;
  return perm;
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
    // "pte" points to the entry of current_percpu level
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
        *pte = PA2PTE(pt) | PTE_V;
      } else // returns NULL, if alloc == 0, or no more physical page remains
        return 0;
    }
  }

  // return a PTE which contains phisical address of a page
  return pt + PX(0, va);
}

//
// look up a virtual page address, return the physical page address or 0 if not
// mapped.
//
uint64 lookup_pa(pagetable_t pagetable, uint64 va) {
  pte_t *pte;
  uint64 pa;

  if (va >= MAXVA)
    return 0;

  pte = page_walk(pagetable, va, 0);
  if (pte == 0 || (*pte & PTE_V) == 0 ||
      ((*pte & PTE_R) == 0 && (*pte & PTE_W) == 0))
    return 0;
  pa = PTE2PA(*pte);

  return pa;
}

/* --- kernel page table part --- */
// _etext is defined in kernel.lds, it points to the address after text and
// rodata segments.
extern char _etext[];

// pointer to kernel page director
pagetable_t g_kernel_pagetable;

//
// maps virtual address [va, va+sz] to [pa, pa+sz] (for kernel).
//
void kern_vm_map(pagetable_t page_dir, uint64 va, uint64 pa, uint64 sz,
                 int perm) {
  // map_pages is defined in kernel/vmm.c
  if (map_pages(page_dir, va, sz, pa, perm) != 0)
    panic("kern_vm_map");
}

//
// kern_vm_init() constructs the kernel page table.
//
void kern_vm_init(void) {
  pagetable_t t_page_dir;
  t_page_dir = (pagetable_t)Alloc_page();
  // 首先分配一个页当内核的页表

  // map virtual address [KERN_BASE, _etext] to physical address [DRAM_BASE,
  // DRAM_BASE+(_etext - KERN_BASE)], to maintin (direct) text section kernel
  // address mapping.
  kern_vm_map(t_page_dir, KERN_BASE, KERN_BASE, (uint64)_etext - KERN_BASE,
              prot_to_type(PROT_READ | PROT_EXEC, 0));

  sprint("KERN_BASE 0x%lx\n", lookup_pa(t_page_dir, KERN_BASE));

  // also (direct) map remaining address space, to make them accessable from
  // kernel. this is important when kernel needs to access the memory content of
  // user's app without copying pages between kernel and user spaces.
  kern_vm_map(t_page_dir, (uint64)_etext, (uint64)_etext,
              PHYS_TOP - (uint64)_etext,
              prot_to_type(PROT_READ | PROT_WRITE, 0));

  sprint("physical address of _etext is: 0x%lx\n",
         lookup_pa(t_page_dir, (uint64)_etext));

  g_kernel_pagetable = t_page_dir;
}

//
// unmap virtual address [va, va+size] from the user app.
// reclaim the physical pages if free!=0
//
void user_vm_unmap(pagetable_t page_dir, uint64 va, uint64 size, int free) {
  pte_t *pte;
  uint64 firstPage = ROUNDDOWN(va, PGSIZE);
  uint64 lastPage = ROUNDDOWN(va + size - 1, PGSIZE);
  if (lastPage <= va)
    for (; firstPage <= lastPage; firstPage += PGSIZE) {
      if ((pte = page_walk(page_dir, firstPage, 1)) == 0)
        panic("user_vm_unmap failed to walk page table for va (0x%lx)",
              firstPage);
      if (!(*pte & PTE_V))
        panic("user_vm_unmap fails on unmapping va (0x%lx)", firstPage);
      if (free) {
        free_page((void *)PTE2PA(*pte));
      }
      *pte = 0;
    }
}
