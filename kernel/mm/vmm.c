/*
 * virtual address mapping related functions.
 */

#include <kernel/mm_struct.h>
#include <kernel/vmm.h>
#include <kernel/pagetable.h>
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

    *pte = PA2PPN(pa) | perm | PTE_V;
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


/* --- kernel page table part --- */
// _etext is defined in kernel.lds, it points to the address after text and
// rodata segments.
extern char _etext[];

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
  pagetable_map(t_page_dir, KERN_BASE, KERN_BASE, (uint64)_etext - KERN_BASE,
              prot_to_type(PROT_READ | PROT_EXEC, 0));

  //sprint("KERN_BASE 0x%lx\n", lookup_pa(t_page_dir, KERN_BASE));

  // also (direct) map remaining address space, to make them accessable from
  // kernel. this is important when kernel needs to access the memory content of
  // user's app without copying pages between kernel and user spaces.
  pagetable_map(t_page_dir, (uint64)_etext, (uint64)_etext,
              PHYS_TOP - (uint64)_etext,
              prot_to_type(PROT_READ | PROT_WRITE, 0));

  //sprint("physical address of _etext is: 0x%lx\n",lookup_pa(t_page_dir, (uint64)_etext));

  g_kernel_pagetable = t_page_dir;
}