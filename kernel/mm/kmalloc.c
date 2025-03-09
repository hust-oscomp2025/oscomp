/**
 * @file kmalloc.c
 * @brief Kernel memory allocation implementation
 *
 * This file implements the kernel memory allocation interface using
 * slab allocator for small allocations and page allocator for large
 * allocations.
 */

#include <kernel/mm/kmalloc.h>
#include <kernel/mm/page.h>
#include <kernel/mm/slab.h>

#include <spike_interface/spike_utils.h>
#include <util/atomic.h>
#include <util/list.h>
#include <util/spinlock.h>
#include <util/string.h>

/**
 * Memory allocation header structure
 * Placed before each allocated memory block to track size
 */
struct kmalloc_header {
  uint32_t size;  // Size of allocation (not including header)
  uint32_t flags; // Allocation flags
};

// Flag to mark large allocations (page-based)
#define KMALLOC_LARGE 0x01
// Magic value to detect corruption
#define KMALLOC_MAGIC 0xA110C8ED

// Size of the allocation header
#define HEADER_SIZE sizeof(struct kmalloc_header)

// Head of large allocation tracking list
static spinlock_t kmalloc_lock = SPINLOCK_INIT;

/**
 * @brief Initialize kernel memory allocator
 */
void kmem_init(void) {
  sprint("Initializing kernel memory allocator...\n");

  // Initialize spinlock
  spinlock_init(&kmalloc_lock);

  // Initialize slab allocator
  slab_init();

  sprint("Kernel memory allocator initialized\n");
}

/**
 * @brief Get header from user pointer
 */
static inline struct kmalloc_header *ptr_to_header(void *ptr) {
  return (struct kmalloc_header *)((char *)ptr - HEADER_SIZE);
}

/**
 * @brief Get user pointer from header
 */
static inline void *header_to_ptr(struct kmalloc_header *header) {
  return (void *)((char *)header + HEADER_SIZE);
}

/**
 * @brief Allocate kernel memory
 */
void *kmalloc(size_t size) {
  if (size == 0)
    return NULL;

  void *mem = NULL;
  struct kmalloc_header *header = NULL;

  // Round up to multiple of 8 bytes for alignment
  size = (size + 7) & ~7;

  // Calculate total size including header
  size_t total_size = size + HEADER_SIZE;

  // For small allocations (up to 2048 bytes), use slab allocator
  if (total_size <= 2048) {
    header = slab_alloc(total_size);
    if (header) {
      header->size = size;
      header->flags = KMALLOC_MAGIC;
      mem = header_to_ptr(header);
    }
  } else {
    // For large allocations, use page allocator
		// 目前只支持最大分配一页
    unsigned int pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;
    struct page *page = alloc_pages(pages - 1); // Convert order to pages

    if (page) {
      header = page_to_virt(page);
      header->size = size;
      header->flags = KMALLOC_MAGIC | KMALLOC_LARGE;
      mem = header_to_ptr(header);
    }
  }

  return mem;
}

/**
 * @brief Free kernel memory
 */
void kfree(void *ptr) {
  if (!ptr)
    return;
	sprint("calling kfree with ptr=%lx\n",ptr);
  // Get allocation header
  struct kmalloc_header *header = ptr_to_header(ptr);

  // Verify magic value for corruption detection
  if ((header->flags & KMALLOC_MAGIC) != KMALLOC_MAGIC) {
    panic("kfree: corrupt header or invalid pointer 0x%lx\n", (uint64)ptr);
  }

  // Check if this is a large allocation
  if (header->flags & KMALLOC_LARGE) {
    // Calculate number of pages used
    size_t total_size = header->size + HEADER_SIZE;
    unsigned int pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

    // Free the physical pages
    struct page *page = virt_to_page(header);
    if (page) {
      // Free the pages - for now we only support single page allocations
      // This would need to be expanded if we fully support multi-page
      // allocations
      free_pages(page, 0); // Order 0 = 1 page
    }
  } else {
    // Free small allocation from slab
    if (!slab_free(header)) {
      panic("kfree: failed to free slab allocation at 0x%lx\n", (uint64)header);
    }
  }
}

/**
 * @brief Allocate and zero kernel memory
 */
void *kzalloc(size_t size) {
  void *ptr = kmalloc(size);
  if (ptr) {
    memset(ptr, 0, size);
  }
  return ptr;
}

/**
 * @brief Get size of allocated memory block
 */
size_t ksize(void *ptr) {
  if (!ptr)
    return 0;

  // Simply read size from header
  struct kmalloc_header *header = ptr_to_header(ptr);

  // Verify magic value for corruption detection
  if ((header->flags & KMALLOC_MAGIC) != KMALLOC_MAGIC) {
    panic("ksize: corrupt header or invalid pointer 0x%lx\n", (uint64)ptr);
  }

  return header->size;
}

/**
 * @brief Resize an allocated memory block
 */
void *krealloc(void *ptr, size_t new_size) {
  // Special cases
  if (!ptr)
    return kmalloc(new_size);
  if (new_size == 0) {
    kfree(ptr);
    return NULL;
  }

  // Get current size from header
  size_t old_size = ksize(ptr);

  // If new size is smaller than or equal to current size, just return the same
  // pointer
  if (new_size <= old_size) {
    return ptr;
  }

  // Allocate new block
  void *new_ptr = kmalloc(new_size);
  if (!new_ptr)
    return NULL;

  // Copy data from old block to new block
  memcpy(new_ptr, ptr, old_size);

  // Free old block
  kfree(ptr);

  return new_ptr;
}

/**
 * @brief Print memory allocator statistics
 */
void kmalloc_stats(void) {
  sprint("Kernel memory allocator statistics:\n");

  // Print slab statistics
  slab_stats();

  // Print page statistics
  sprint("Free page count: %d\n", get_free_page_count());
}