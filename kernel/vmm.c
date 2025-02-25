/*
 * virtual address mapping related functions.
 */

#include "vmm.h"
#include "global.h"
#include "memlayout.h"
#include "pmm.h"
#include "process.h"
#include "riscv.h"
#include "spike_interface/spike_utils.h"
#include "util/functions.h"
#include "util/string.h"
#include "util/types.h"

/* --- utility functions for virtual address mapping --- */
//
// establish mapping of virtual address [va, va+size] to phyiscal address [pa,
// pa+size] with the permission of "perm".
//
//
int map_pages(pagetable_t page_dir, uint64 va, uint64 size, uint64 pa,
              int perm) {
  uint64 first, last;
  pte_t *pte;

  for (first = ROUNDDOWN(va, PGSIZE), last = ROUNDDOWN(va + size - 1, PGSIZE);
       first <= last; first += PGSIZE, pa += PGSIZE) {
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
    // macro "PX" gets the PTE index in page table of current level
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
  kern_vm_map(t_page_dir, KERN_BASE, DRAM_BASE, (uint64)_etext - KERN_BASE,
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

  kernel_heap_head.next = NULL;
  kernel_heap_head.prev = NULL;
  kernel_heap_head.free = 0;
  kernel_heap_head.size = 0;

  g_kernel_pagetable = t_page_dir;
}

/* --- user page table part --- */
//
// convert and return the corresponding physical address of a virtual address
// (va) of application.
//
void *user_va_to_pa(pagetable_t page_dir, void *va) {
  // TODO (lab2_1): implement user_va_to_pa to convert a given user virtual
  // address "va" to its corresponding physical address, i.e., "pa". To do it,
  // we need to walk through the page table, starting from its directory
  // "page_dir", to locate the PTE that maps "va". If found, returns the "pa" by
  // using: pa = PYHS_ADDR(PTE) + (va & (1<<PGSHIFT -1)) Here, PYHS_ADDR() means
  // retrieving the starting address (4KB aligned), and (va & (1<<PGSHIFT -1))
  // means computing the offset of "va" inside its page. Also, it is possible
  // that "va" is not mapped at all. in such case, we can find invalid PTE, and
  // should return NULL. panic( "You have to implement user_va_to_pa (convert
  // user va to pa) to print messages in lab2_1.\n" );
  pte_t *pte = page_walk(page_dir, (uint64)va, 0);
  uint64 offset = (uint64)va & ((1UL << PGSHIFT) - 1);
  if (pte == 0)
    return NULL;
  return (void *)(PTE2PA(*pte) + offset);
}

//
// maps virtual address [va, va+sz] to [pa, pa+sz] (for user application).
//
void user_vm_map(pagetable_t page_dir, uint64 va, uint64 size, uint64 pa,
                 int perm) {
  if (map_pages(page_dir, va, size, pa, perm) != 0) {
    panic("fail to user_vm_map .\n");
  }
}

//
// unmap virtual address [va, va+size] from the user app.
// reclaim the physical pages if free!=0
//
void user_vm_unmap(pagetable_t page_dir, uint64 va, uint64 size, int free) {
  // TODO (lab2_2): implement user_vm_unmap to disable the mapping of the
  // virtual pages in [va, va+size], and free the corresponding physical pages
  // used by the virtual addresses when if 'free' (the last parameter) is not
  // zero. basic idea here is to first locate the PTEs of the virtual pages, and
  // then reclaim (use free_page() defined in pmm.c) the physical pages. lastly,
  // invalidate the PTEs. as naive_free reclaims only one page at a time, you
  // only need to consider one page to make user/app_naive_malloc to behave
  // correctly. panic( "You have to implement user_vm_unmap to free pages using
  // naive_free in lab2_2.\n" );
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
// 计算用户请求的内存大小，跳过堆元数据部分
#define USER_MEM_SIZE(size) ((size) + sizeof(heap_block))

void heap_init(process *ps) {
  heap_block *first_block = (heap_block *)Alloc_page();
  first_block->size = PGSIZE - sizeof(heap_block);
  first_block->prev = NULL;
  first_block->next = NULL;
  first_block->free = 1; // 初始块是空闲的
  user_vm_map(ps->pagetable, ps->user_heap.heap_bottom, PGSIZE,
              (uint64)first_block, prot_to_type(PROT_WRITE | PROT_READ, 1));
  ps->user_heap.heap_top += PGSIZE;
  ps->mapped_info[HEAP_SEGMENT].npages++;
}

// 目前只服务大小小于一个页的内存请求
void *vmalloc(size_t size) {
  if (size == 0)
    return NULL;
  int hartid = read_tp();
  process *ps = current[hartid];
  if (ps->user_heap.heap_top == ps->user_heap.heap_bottom) {
    heap_init(ps);
  }

  int required_size = ALIGN(size + sizeof(heap_block), 8);
  heap_block *current_block = (heap_block *)ps->user_heap.heap_bottom;
  heap_block *pa_current_block = user_va_to_pa(ps->pagetable, current_block);
  if (required_size >= PGSIZE) {
    goto heap_alloc;
  }
  // 遍历空闲链表，查找足够大的空闲块
  while (current_block != NULL) {

    /*可以分配*/
    if (pa_current_block->free && pa_current_block->size >= required_size) {
      /*可以分割*/
      if (pa_current_block->size > required_size + sizeof(heap_block)) {
        pa_current_block->size = required_size;
        heap_block *new_block =
            (heap_block *)((uintptr_t)current_block + required_size);
        heap_block *pa_newblock = user_va_to_pa(ps->pagetable, new_block);
        pa_newblock->size =
            pa_current_block->size - required_size - sizeof(heap_block);
        pa_newblock->free = 1;
        pa_newblock->next = pa_current_block->next;
        pa_newblock->prev = current_block;

        
        if (pa_current_block->next != NULL) {
          heap_block *next_block = (heap_block *)pa_current_block->next;
          heap_block *pa_next_block = user_va_to_pa(ps->pagetable, (void*)next_block);
          pa_next_block->prev = new_block;
        }
        pa_current_block->next = new_block;
      }
      pa_current_block->free = 0;
      return (void *)((uintptr_t)current_block + sizeof(heap_block));
    }
    if (pa_current_block->next == NULL) {
    heap_alloc:
      // 遍历到了最后一个节点，仍然没有找到可用的块
      // 新分配一个够大的块，然后再做切割
      int n_pages = (required_size >> 12) + 1;
      uint64 va = ps->user_heap.heap_top;
      // void *va = (void *)(((uint64)current_block >> 12) << 12) + PGSIZE;
      for (uint64 i = 0; i < n_pages; i++) {
        user_vm_map(ps->pagetable, va + i * PGSIZE, PGSIZE,
                    (uint64)Alloc_page(),
                    prot_to_type(PROT_READ | PROT_WRITE, 1));
      }
      ps->user_heap.heap_top += n_pages * PGSIZE;
      ps->mapped_info[HEAP_SEGMENT].npages += n_pages;

      heap_block *pa = user_va_to_pa(ps->pagetable, (void *)va);
      pa->next = NULL;
      pa->size = PGSIZE * n_pages - sizeof(heap_block);
      pa->prev = current_block;
      pa->free = 1;
      pa_current_block->next = (heap_block *)va;

      current_block = (heap_block *)va;
      continue;
    }
    current_block = pa_current_block->next;
    pa_current_block = user_va_to_pa(ps->pagetable, current_block);
  }

  // 如果没有找到合适的块，返回 NULL（表示堆内存不足）
  return NULL;
}

void free(void *ptr) {
  int hartid = read_tp();
  // 如果指针为 NULL，直接返回
  if (ptr == NULL) {
    return;
  }

  // 获取指向 heap_block 的指针，跳过用户数据区域
  heap_block *block = (heap_block *)((uintptr_t)ptr - sizeof(heap_block));
  heap_block *pa_block = user_va_to_pa(current[hartid]->pagetable, block);
  // 如果该块已经是空闲的，说明已经释放过了，直接返回
  if (pa_block->free) {
    return;
  }

  // 标记为已释放
  pa_block->free = 1;

  heap_block *block_prev = pa_block->prev;
  heap_block *pa_block_prev =
      user_va_to_pa(current[hartid]->pagetable, block_prev);
  heap_block *block_next = pa_block->next;
  heap_block *pa_block_next =
      user_va_to_pa(current[hartid]->pagetable, block_next);
  // 合并前面的空闲块
  if (block_prev != NULL && pa_block_prev->free) {
    // 合并前一个空闲块
    pa_block_prev->size += pa_block->size;
    pa_block_prev->next = pa_block->next;
    if (pa_block->next != NULL) {

      pa_block_next->prev = pa_block->prev;
    }
    block = pa_block->prev; // 更新块指针，合并后变成前一个块
  }

  // 合并后面的空闲块
  if (block_next != NULL && pa_block_next->free) {
    // 合并后一个空闲块
    pa_block->size += pa_block_next->size;
    pa_block->next = pa_block_next->next;
    if (block_next != NULL) {
      pa_block_next->prev = block;
    }
  }

  // 合并会自动设置好堆的头指针，所以不需要做更多工作。
  // if (block_prev == NULL) {
  //  current[hartid]->heap = block; // 更新堆的头部
  //}
}

//
// debug function, print the vm space of a process. added @lab3_1
//
void print_proc_vmspace(process *proc) {
  sprint("======\tbelow is the vm space of process%d\t========\n", proc->pid);
  for (int i = 0; i < proc->total_mapped_region; i++) {
    sprint("-va:%lx, npage:%d, ", proc->mapped_info[i].va,
           proc->mapped_info[i].npages);
    switch (proc->mapped_info[i].seg_type) {
    case CODE_SEGMENT:
      sprint("type: CODE SEGMENT");
      break;
    case DATA_SEGMENT:
      sprint("type: DATA SEGMENT");
      break;
    case STACK_SEGMENT:
      sprint("type: STACK SEGMENT");
      break;
    case CONTEXT_SEGMENT:
      sprint("type: TRAPFRAME SEGMENT");
      break;
    case SYSTEM_SEGMENT:
      sprint("type: USER KERNEL STACK SEGMENT");
      break;
    }
    sprint(", mapped to pa:%lx\n",
           lookup_pa(proc->pagetable, proc->mapped_info[i].va));
  }
}
