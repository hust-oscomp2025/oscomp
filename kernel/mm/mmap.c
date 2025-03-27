#include <kernel/types.h>

#include <kernel/mm/mm_struct.h>
#include <kernel/mm/mmap.h>
#include <kernel/mm/vma.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/util/string.h>			//memset
#include <kernel/sprint.h>

/**
 * 将保护标志(PROT_*)转换为页表项标志
 */
uint64 prot_to_type(int32 prot, int32 user) {
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

// Convert PROT_* to VM_* flags
uint64 prot_to_vm_flags(int32 prot) {
  uint64 vm_flags = 0;
  if (prot & PROT_READ)
    vm_flags |= VM_READ | VM_MAYREAD;
  if (prot & PROT_WRITE)
    vm_flags |= VM_WRITE | VM_MAYWRITE;
  if (prot & PROT_EXEC)
    vm_flags |= VM_EXEC | VM_MAYEXEC;
  return vm_flags;
}
/**
 * Convert VM_* flags to page table entry flags
 *
 * @param vm_flags The VM flags (VM_READ, VM_WRITE, etc.)
 * @return Page table entry permission bits
 */
uint64 vm_flags_to_type(uint64 vm_flags) {
  uint64 perm = 0;

  // Convert basic access flags
  if (vm_flags & VM_READ)
    perm |= PTE_R | PTE_A;

  if (vm_flags & VM_WRITE)
    perm |= PTE_W | PTE_D;

  if (vm_flags & VM_EXEC)
    perm |= PTE_X | PTE_A;

  // Set user access bit if it's user memory
  if (vm_flags & VM_USER)
    perm |= PTE_U;

  // Ensure page is valid
  perm |= PTE_V;

  // If no access rights, but page is valid, use special case
  if (!(perm & (PTE_R | PTE_W | PTE_X)))
    perm = PTE_V; // Valid but no access

  return perm;
}

/**
 * Convert page table entry flags to VM_* flags
 *
 * @param pte_flags Page table entry flags
 * @return VM_* flags
 */
uint64 type_to_vm_flags(uint64 pte_flags) {
  uint64 vm_flags = 0;

  if (pte_flags & PTE_R)
    vm_flags |= VM_READ | VM_MAYREAD;

  if (pte_flags & PTE_W)
    vm_flags |= VM_WRITE | VM_MAYWRITE;

  if (pte_flags & PTE_X)
    vm_flags |= VM_EXEC | VM_MAYEXEC;

  if (pte_flags & PTE_U)
    vm_flags |= VM_USER;

  return vm_flags;
}



/**
 * Update memory descriptor boundaries based on the VMA
 */
static void update_mm_boundaries(struct mm_struct *mm, struct vm_area_struct *vma) {
	if (vma->vm_type == VMA_TEXT) {
			if (mm->start_code == 0 || vma->vm_start < mm->start_code)
					mm->start_code = vma->vm_start;
			if (vma->vm_end > mm->end_code)
					mm->end_code = vma->vm_end;
	} else if (vma->vm_type == VMA_DATA) {
			if (mm->start_data == 0 || vma->vm_start < mm->start_data)
					mm->start_data = vma->vm_start;
			if (vma->vm_end > mm->end_data)
					mm->end_data = vma->vm_end;
	}
}

/**
 * Find a free area of virtual memory of specified size
 */
static uint64 find_free_area(struct mm_struct *mm, size_t length) {
	uint64 addr = mm->brk;  // Start from heap break
	
	while (find_vma_intersection(mm, addr, addr + length)) {
			addr = ROUNDUP(addr + PAGE_SIZE, PAGE_SIZE);
	}
	
	return addr;
}



/**
 * Create a new memory mapping
 *
 * @param mm Memory descriptor
 * @param addr Suggested virtual address (0 for auto-allocation)
 * @param length Length of mapping in bytes
 * @param prot Protection flags (PROT_READ/WRITE/EXEC)
 * @param flags Mapping flags (MAP_PRIVATE/SHARED/FIXED/etc)
 * @param file Optional file for file-backed mapping (NULL for anonymous)
 * @param pgoff File offset in pages or physical address for direct mapping
 * @return Mapped virtual address or negative error code
 */
uint64 do_mmap(struct mm_struct *mm, uint64 addr, size_t length, int32 prot,
               uint64 flags, struct file *file, uint64 pgoff) {
  if (!mm || length == 0)
    return -EINVAL;

  // Round length to page boundary
  length = ROUNDUP(length, PAGE_SIZE);

  // Determine VMA type based on flags and file
  enum vma_type type;
  if (file)
    type = VMA_FILE;
  else if (flags & MAP_ANONYMOUS)
    type = VMA_ANONYMOUS;
  else if (prot & PROT_EXEC)
    type = VMA_TEXT;
  else
    type = VMA_STACK; // Special case for stack

  // Convert protection flags to vm_flags
  uint64 vm_flags = 0;
  if (prot & PROT_READ)
    vm_flags |= VM_READ | VM_MAYREAD;
  if (prot & PROT_WRITE)
    vm_flags |= VM_WRITE | VM_MAYWRITE;
  if (prot & PROT_EXEC)
    vm_flags |= VM_EXEC | VM_MAYEXEC;

  // Apply mapping flags
  if (flags & MAP_SHARED)
    vm_flags |= VM_SHARED;
  if (flags & MAP_PRIVATE)
    vm_flags |= VM_PRIVATE;
  if (flags & MAP_GROWSDOWN)
    vm_flags |= VM_GROWSDOWN;

  // Find suitable address if needed
  if (addr == 0) {
    addr = find_free_area(mm, length);
  } else if (flags & MAP_FIXED) {
    if (find_vma_intersection(mm, addr, addr + length))
      return -EINVAL;
  }

  // Create the VMA
  struct vm_area_struct *vma =
      vm_area_setup(mm, addr, length, type, prot, vm_flags);
  if (!vma)
    return -ENOMEM;

  // Handle file-backed mapping
  if (file) {
    vma->vm_file = file;
    vma->vm_pgoff = pgoff;
  }

  // Pre-populate pages if requested
  if (flags & MAP_POPULATE) {
		populate_vma(vma,addr,length,prot);
  }

  // Update code/data boundaries if needed
  if (type == VMA_TEXT) {
    if (mm->start_code == 0 || addr < mm->start_code)
      mm->start_code = addr;
    if (addr + length > mm->end_code)
      mm->end_code = addr + length;
  } else if (type == VMA_DATA || type == VMA_FILE) {
    if (mm->start_data == 0 || addr < mm->start_data)
      mm->start_data = addr;
    if (addr + length > mm->end_data)
      mm->end_data = addr + length;
  }

  // Return mapped address
  return addr;
}

/**
 * do_unmap - Unmap a region from the process address space
 * @mm: The memory descriptor
 * @start: Start address to unmap
 * @len: Length of the region to unmap
 *
 * This function unmaps the specified memory region from the process address
 * space. It handles VMA splitting, page freeing, and page table updates.
 *
 * Return: 0 on success, negative error code on failure
 */
int32 do_unmap(struct mm_struct *mm, uint64 start, size_t len) {
  uint64 end;
  struct vm_area_struct *vma, *prev, *next, *free_vma;
  int32 count = 0;
  int32 ret = 0;

  /* Sanity checks */
  if (!mm || !len)
    return -EINVAL;

  if ((start & ~PAGE_MASK) || (len & ~PAGE_MASK))
    return -EINVAL; /* Must be page-aligned */

  /* Calculate end address */
  end = start + len;
  if (end < start)
    return -EINVAL; /* Overflow check */

  /* Find first overlapping VMA */
  vma = find_vma(mm, start);
  if (!vma || vma->vm_start >= end)
    return 0; /* No overlap - nothing to do */

  /* Handle the case where we need to split the first VMA */
  if (start > vma->vm_start) {
    /* Create a new VMA for the first part */
    free_vma = vm_area_setup(mm, vma->vm_start, start - vma->vm_start,
                             vma->vm_type, vma->vm_prot, vma->vm_flags);
    if (!free_vma)
      return -ENOMEM;

    /* Copy relevant pages from original VMA to new VMA */
    int32 orig_start_idx = 0;
    int32 new_vma_page_count = (start - vma->vm_start) / PAGE_SIZE;
    for (int32 i = 0; i < new_vma_page_count; i++) {
      free_vma->pages[i] = vma->pages[i];
      vma->pages[i] = NULL;
    }

    /* Shift remaining pages in original VMA */
    int32 remain_pages = vma->page_count - new_vma_page_count;
    if (remain_pages > 0) {
      for (int32 i = 0; i < remain_pages; i++) {
        vma->pages[i] = vma->pages[i + new_vma_page_count];
      }
      memset(&vma->pages[remain_pages], 0,
             (vma->page_count - remain_pages) * sizeof(struct page *));
    }

    /* Update original VMA start address */
    vma->vm_start = start;
  }

  /* Handle the case where we need to split the last VMA */
  next = find_vma(mm, end);
  if (next && next->vm_start < end) {
    if (end < next->vm_end) {
      /* Create a new VMA for the part after 'end' */
      free_vma = vm_area_setup(mm, end, next->vm_end - end, next->vm_type,
                               next->vm_prot, next->vm_flags);
      if (!free_vma)
        return -ENOMEM;

      /* Copy relevant pages */
      int32 split_idx = (end - next->vm_start) / PAGE_SIZE;
      int32 remain_pages = next->page_count - split_idx;
      for (int32 i = 0; i < remain_pages; i++) {
        free_vma->pages[i] = next->pages[i + split_idx];
        next->pages[i + split_idx] = NULL;
      }

      /* Update end address for the last VMA to be unmapped */
      next->vm_end = end;
    }
  }

  /* Free all VMAs in the range */
  list_for_each_entry_safe(vma, next, &mm->vma_list, vm_list) {
    if (vma->vm_start >= end)
      break;

    if (vma->vm_end <= start)
      continue;

    /* Calculate region to unmap in this VMA */
    uint64 unmap_start = MAX(start, vma->vm_start);
    uint64 unmap_end = MIN(end, vma->vm_end);

    /* Free physical pages and unmap */
    int32 start_idx = (unmap_start - vma->vm_start) / PAGE_SIZE;
    int32 end_idx = (unmap_end - vma->vm_start + PAGE_SIZE - 1) / PAGE_SIZE;

    for (int32 i = start_idx; i < end_idx && i < vma->page_count; i++) {
      if (vma->pages[i]) {
        uint64 page_va = vma->vm_start + (i * PAGE_SIZE);
        pgt_unmap(mm->pagetable, page_va, PAGE_SIZE, 1);
        put_page(vma->pages[i]);
        vma->pages[i] = NULL;
      }
    }

    /* Remove this VMA */
    list_del(&vma->vm_list);
    if (vma->pages)
      kfree(vma->pages);
    kfree(vma);
    mm->map_count--;
    count++;
  }

  /* Flush TLB */
  flush_tlb();

  return 0;
}

/**
 * Extend or shrink the process heap
 * 
 * @param mm The memory descriptor
 * @param new_brk The new brk address (NOT an increment)
 * @return The new brk address on success, negative error code on failure
 */
uint64 do_brk(struct mm_struct *mm, uint64 new_brk) {
	if (unlikely(!mm))
			return -EINVAL;
	
	// If new_brk is 0, just return the current brk (used by sbrk(0))
	if (new_brk == 0)
			return mm->brk;
	
	// New brk must be page-aligned
	new_brk = ROUNDUP(new_brk, PAGE_SIZE);
	uint64 old_brk = mm->brk;
	
	// Validate: new brk cannot be below the start of heap region
	if (unlikely(new_brk < mm->start_brk))
			return -EINVAL;
	
	// No change case - just return current brk
	if (new_brk == old_brk)
			return old_brk;
	
	/* HEAP EXPANSION */
	if (new_brk > old_brk) {
			// Check if any existing VMA overlaps with the expansion area
			if (find_vma_intersection(mm, old_brk, new_brk)) {
					sprint("mm_brk: expansion area overlaps with existing VMA\n");
					return -ENOMEM;
			}
			
			// Find existing heap VMA or create one if it doesn't exist
			struct vm_area_struct *vma = find_vma(mm, old_brk - 1);
			
			if (!vma || vma->vm_end != old_brk || vma->vm_type != VMA_HEAP) {
					// No existing heap VMA or it doesn't end at current brk
					// Create a new VMA for the expanded area
					vma = vm_area_setup(mm, old_brk, new_brk - old_brk, VMA_HEAP, 
														PROT_READ | PROT_WRITE, 
														VM_GROWSUP | VM_PRIVATE | VM_USER);
					if (!vma) {
							sprint("mm_brk: failed to create new heap VMA\n");
							return -ENOMEM;
					}
			} else {
					// Extend the existing heap VMA
					vma->vm_end = new_brk;
					
					// Ensure the pages array can handle the extended size
					uint64 old_npages = vma->page_count;
					uint64 new_npages = (new_brk - vma->vm_start + PAGE_SIZE - 1) / PAGE_SIZE;
					
					if (new_npages > old_npages) {
							// Reallocate the pages array
							struct page **new_pages = 
									kmalloc(new_npages * sizeof(struct page *));
							if (!new_pages) {
									sprint("mm_brk: failed to allocate pages array\n");
									vma->vm_end = old_brk;  // Restore old size
									return -ENOMEM;
							}
							
							// Initialize the new array
							memset(new_pages, 0, new_npages * sizeof(struct page *));
							
							// Copy existing pages if any
							if (vma->pages) {
									memcpy(new_pages, vma->pages, 
											 old_npages * sizeof(struct page *));
									kfree(vma->pages);
							}
							
							vma->pages = new_pages;
							vma->page_count = new_npages;
					}
			}
	}
	/* HEAP CONTRACTION */
	else {
			// Find the VMA that contains the area to be unmapped
			struct vm_area_struct *vma = find_vma(mm, new_brk);
			
			if (vma && vma->vm_start < new_brk && vma->vm_type == VMA_HEAP) {
					// Free the pages in the contracted region
					uint64 page_size = PAGE_SIZE;
					for (uint64 addr = ROUNDUP(new_brk, page_size); 
							 addr < old_brk; 
							 addr += page_size) {
							
							int32 page_idx = (addr - vma->vm_start) / page_size;
							if (page_idx >= 0 && page_idx < vma->page_count && vma->pages[page_idx]) {
									// Unmap and free this page
									pgt_unmap(mm->pagetable, addr, page_size, 1);
									put_page(vma->pages[page_idx]);
									vma->pages[page_idx] = NULL;
							}
					}
					
					// Shrink the VMA
					vma->vm_end = new_brk;
					
					// Optionally reallocate the pages array to save memory
					// This is less critical and could be omitted for simplicity
			}
			// If there's no VMA at the new_brk point, nothing special to do
	}
	
	// Update the brk value in mm_struct
	mm->brk = new_brk;
	return new_brk;
}


/**
 * do_protect - Change protection bits of memory pages
 * @mm: The memory descriptor
 * @start: Start address (must be page-aligned)
 * @len: Length to protect (will be rounded up to page boundary)
 * @prot: New protection bits (PROT_READ/WRITE/EXEC)
 *
 * This function changes the access protections for user pages in a range,
 * updating both page tables and VMA permissions accordingly. It validates
 * that the requested protections are allowed by the VMA's mayXXX flags.
 *
 * Return: 0 on success, negative error code on failure
 */
int32 do_protect(struct mm_struct *mm, __page_aligned uint64 start, size_t len, int32 prot) {
	uint64 end;
	struct vm_area_struct *vma;
	uint64 current_addr;
	
	/* Parameter validation */
	if (unlikely(!mm))
			return -EINVAL;
	
	if (unlikely(start & (PAGE_SIZE - 1)))
			return -EINVAL;  /* Start must be page-aligned */
	
	if (len == 0)
			return 0;
			
	/* Round up length to page boundary */
	len = ROUNDUP(len, PAGE_SIZE);
	end = start + len;
	
	/* Check for overflow */
	if (end < start)
			return -EINVAL;
			
	/* Convert PROT_* to VM_* flags for validation */
	uint64 vm_flags = 0;
	if (prot & PROT_READ)
			vm_flags |= VM_READ;
	if (prot & PROT_WRITE)
			vm_flags |= VM_WRITE;
	if (prot & PROT_EXEC)
			vm_flags |= VM_EXEC;
	
	/* Convert protection to page table flags */
	int32 is_user = !mm->is_kernel_mm;
	uint64 pte_perm = prot_to_type(prot, is_user);
	
	/* Walk through all VMAs in the range */
	current_addr = start;
	while (current_addr < end) {
			vma = find_vma(mm, current_addr);
			
			/* Gap in the memory map or beyond end */
			if (!vma || vma->vm_start >= end)
					return -ENOMEM;
					
			/* Verify permissions are allowed by VMA */
			if ((prot & PROT_READ) && !(vma->vm_flags & VM_MAYREAD))
					return -EACCES;
			if ((prot & PROT_WRITE) && !(vma->vm_flags & VM_MAYWRITE))
					return -EACCES;
			if ((prot & PROT_EXEC) && !(vma->vm_flags & VM_MAYEXEC))
					return -EACCES;
			
			/* Calculate address range within this VMA */
			uint64 change_start = MAX(current_addr, vma->vm_start);
			uint64 change_end = MIN(end, vma->vm_end);
			
			/* Update permissions in VMA */
			vma->vm_prot = prot;
			vma->vm_flags &= ~(VM_READ | VM_WRITE | VM_EXEC);
			vma->vm_flags |= vm_flags;
			
			/* Update page table entries */
			for (uint64 addr = change_start; addr < change_end; addr += PAGE_SIZE) {
					pte_t *pte = page_walk(mm->pagetable, addr, 0);
					if (pte && (*pte & PTE_V)) {
							uint64 pa = PTE2PA(*pte);
							*pte = PA2PPN(pa) | pte_perm | PTE_V;
					}
			}
			
			/* Move to next VMA */
			current_addr = vma->vm_end;
	}
	
	/* Flush TLB after updating all pages */
	flush_tlb();
	
	return 0;
}