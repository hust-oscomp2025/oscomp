#include <kernel/mm/vma.h>

static int insert_vm_struct(struct mm_struct *mm, struct vm_area_struct *vma);
static int vma_alloc_page_array(struct vm_area_struct *vma);
static void vma_init(struct vm_area_struct *vma, struct mm_struct *mm,
                     uint64 start, uint64 end,enum vma_type type, int prot, uint64 flags);
static struct vm_area_struct *alloc_vma();

void free_vma(struct vm_area_struct *vma) {
  if (vma->pages) {
    for (int i = 0; i < vma->page_count; i++) {
      if (vma->pages[i]) {
        free_page(vma->pages[i]);
      }
    }
    kfree(vma->pages);
  }
  list_del(&vma->vm_list);
  kfree(vma);
}

/**
 * vm_area_setup - Create and insert a fully configured VMA
 * @mm: The memory descriptor
 * @addr: Start address
 * @len: Length of the region
 * @type: VMA type (code, data, etc)
 * @prot: Page protection bits
 * @flags: VMA flags
 *
 * Returns: The created VMA or NULL on failure
 */
struct vm_area_struct *vm_area_setup(struct mm_struct *mm, uint64 addr,
                                     uint64 len, enum vma_type type, int prot,
                                     uint64 flags) {
  struct vm_area_struct *vma;

  if (!mm || addr >= addr + len)
    return NULL;

  // Create VMA
  vma = alloc_vma(mm);
  if (!vma)
    return NULL;

  // Initialize VMA
  vma_init(vma, mm, addr, addr + len,type,prot, flags);


  // Allocate page tracking array
  if (vma_alloc_page_array(vma) != 0) {
    kfree(vma);
    return NULL;
  }

  // Insert VMA
  if (insert_vm_struct(mm, vma) != 0) {
    if (vma->pages)
      kfree(vma->pages);
    kfree(vma);
    return NULL;
  }

  // Update memory segment info based on type
  if (type == VMA_TEXT) {
    if (mm->start_code == 0 || addr < mm->start_code)
      mm->start_code = addr;
    if (addr + len > mm->end_code)
      mm->end_code = addr + len;
  } else if (type == VMA_DATA) {
    if (mm->start_data == 0 || addr < mm->start_data)
      mm->start_data = addr;
    if (addr + len > mm->end_data)
      mm->end_data = addr + len;
  }

  return vma;
}

/**
 * populate_vma
 * Populate a VMA with physical pages (used with MAP_POPULATE)
 * 也可以只填充vma的部分页
 */
int populate_vma(struct vm_area_struct *vma, uint64 addr, size_t length,
                 int prot) {
  for (size_t offset = 0, page_idx = offset / PAGE_SIZE; offset < length;
       offset += PAGE_SIZE, page_idx++) {
    if (vma->pages[page_idx]) {
      continue;
    }
    struct page *page = alloc_page();
    if (unlikely(!page)) {
      do_unmap(vma->vm_mm, addr, offset);
      return -ENOMEM;
    }
    vma->pages[page_idx] = page;

    uint64 pa = page_to_virt(page);
    int ret = pgt_map_page(vma->vm_mm->pagetable, addr + offset, (uint64)pa,
                           prot_to_type(prot, vma->vm_flags & VM_USER));
    if (unlikely(ret)) {
      free_page(page);
      do_unmap(vma->vm_mm, addr, offset);
      return -ENOMEM;
    }
  }

  return 0;
}

/**
 * alloc_vma - Allocate a VMA structure
 * @mm: The memory descriptor
 *
 * Returns: A newly allocated vm_area_struct, or NULL on failure
 */
static struct vm_area_struct *alloc_vma() {
  struct vm_area_struct *vma;

  vma = (struct vm_area_struct *)kmalloc(sizeof(struct vm_area_struct));
  if (!vma)
    return NULL;

  memset(vma, 0, sizeof(struct vm_area_struct));
  spinlock_init(&vma->vma_lock);

  return vma;
}

/**
 * vma_init - Initialize a VMA with basic properties
 * @vma: The VMA to initialize
 * @start: Start address
 * @end: End address
 * @flags: VMA flags
 *
 * This sets up common fields but doesn't allocate page arrays.
 */
static void vma_init(struct vm_area_struct *vma, struct mm_struct *mm,
                     uint64 start, uint64 end,enum vma_type type, int prot, uint64 flags) {
  vma->vm_start = start;
  vma->vm_end = end;
  vma->vm_flags = flags;
  vma->vm_mm = mm;

  // Calculate page count - moved to a separate step
  vma->page_count = (end - start + PAGE_SIZE - 1) / PAGE_SIZE;
	vma->vm_type = type;

  // Set protection bits
  if (prot & PROT_READ)
    vma->vm_flags |= VM_READ | VM_MAYREAD;
  if (prot & PROT_WRITE)
    vma->vm_flags |= VM_WRITE | VM_MAYWRITE;
  if (prot & PROT_EXEC)
    vma->vm_flags |= VM_EXEC | VM_MAYEXEC;
}

/**
 * vma_alloc_page_array - Allocate page tracking array for VMA
 * @vma: The VMA to allocate pages for
 *
 * Returns: 0 on success, -ENOMEM on failure
 */
static int vma_alloc_page_array(struct vm_area_struct *vma) {
  if (vma->page_count > 0) {
    vma->pages = kmalloc(vma->page_count * sizeof(struct page *));
    if (!vma->pages)
      return -ENOMEM;

    memset(vma->pages, 0, vma->page_count * sizeof(struct page *));
  }
  return 0;
}

/**
 * insert_vm_struct - Insert a VMA into the mm's VMA list
 * @mm: The memory descriptor
 * @vma: The VMA to insert
 *
 * Returns: 0 on success, -ENOMEM if VMA overlaps with existing ones
 */
static int insert_vm_struct(struct mm_struct *mm, struct vm_area_struct *vma) {
  // Check for overlaps
  if (find_vma_intersection(mm, vma->vm_start, vma->vm_end)) {
    return -ENOMEM;
  }

  // Add to VMA list
  list_add(&vma->vm_list, &mm->vma_list);
  mm->map_count++;

  return 0;
}
