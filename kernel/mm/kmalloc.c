/**
 * @file kmalloc.c
 * @brief Kernel memory allocation implementation
 *
 * This file implements the kernel memory allocation interface using
 * slab allocator for small allocations and page allocator for large
 * allocations.
 */

#include <kernel/mmu.h>

#include <kernel/util/print.h>
#include <kernel/util.h>
#include <kernel/syscall/syscall.h>

/**
 * Memory allocation header structure
 * Placed before each allocated memory block to track size
 */
struct kmalloc_header {
  uint32_t size;  // Size of allocation (not including header)
  uint32_t magic; // Magic value to detect corruption
};

// Magic value to detect corruption
#define KMALLOC_MAGIC 0xA110C8ED
#define HEADER_SIZE sizeof(struct kmalloc_header)

static spinlock_t kmalloc_lock = SPINLOCK_INIT;
extern struct mm_struct init_mm;


// 在kernel初始化中被调用
void kmem_init(void) {
  kprintf("Initializing kernel memory allocator...\n");

  // Initialize spinlock
  spinlock_init(&kmalloc_lock);

  slab_init();
  pagetable_server_init();

  kprintf("Kernel memory allocator initialized\n");
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
  kprintf("kmalloc: request mem size = %d\n",size);
  if (size == 0)
    return NULL;

	 void *mem = NULL;

  // Round up to multiple of 8 bytes for alignment
  size = (size + 7) & ~7;

  // Calculate total size including header
  size_t total_size = size + HEADER_SIZE;

  // For small allocations (up to 2048 bytes), use slab allocator
  if (total_size <= 2048) {
    //kprintf("kmalloc: small\n");

    struct kmalloc_header *header = slab_alloc(total_size);
    if (header) {
      header->size = size;
      header->magic = KMALLOC_MAGIC;
      mem = header_to_ptr(header);
    }
  } else {
    //kprintf("kmalloc: large\n");
    // For large allocations, use page allocator without headers
    vaddr_t ret = mmap_file(&init_mm, 0, size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, NULL, 0);
					
	kprintf("kmalloc:mmap_file ret=%lx\n", ret);
    struct page *page = addr_to_page(ret);

    if (page) {
      page->kmalloc_size = size;
      // Store the actual requested size in the page's mapping field
      //mem = (void*)page->paddr;
	  mem = (void*)ret;
    }else{
		return NULL;
	}
  }
  //kprintf("kmalloc: end\n");
  kprintf("kmalloc: allocated %d bytes at %lx\n", size, mem);
  return mem;
}

/**
 * @brief Free kernel memory
 */
 void kfree(kptr_t ptr) {
  if (!ptr)
    return;

  kprintf("calling kfree with ptr=%lx\n", ptr);

  // Check if this is a page allocation (page-aligned pointer)
  if (((uint64)ptr & (PAGE_SIZE - 1)) == 0) {
    // This is a page allocation
    struct page *page = addr_to_page((paddr_t)ptr);
    if (unlikely(!page)) {
      panic("kfree: invalid pointer 0x%lx\n", (uint64)ptr);
    }
		do_unmap(&init_mm, (uint64)ptr, page->kmalloc_size);

    return;
  } else {
    // Small allocation with header
    struct kmalloc_header *header = ptr_to_header(ptr);

    // Verify magic value for corruption detection
    if (header->magic != KMALLOC_MAGIC) {
      panic("kfree: corrupt header or invalid pointer 0x%lx\n", (uint64)ptr);
    }

    // Free small allocation from slab
    if (!slab_free(header)) {
      panic("kfree: failed to free allocation at 0x%lx\n", (uint64)header);
    }
  }
}


void *kcalloc(size_t n, size_t size)
{
    size_t total_size;
    void *ret;

    // 检查乘法溢出
    if (size && n > (~(size_t)0) / size)
        return NULL;

    total_size = n * size;

    // 如果总大小为0，返回一个特殊值或NULL
    if (total_size == 0)
        return NULL;  // 或者返回一个特殊的非NULL值，取决于您的需求

    // 使用您的kmalloc分配内存
    ret = kmalloc(total_size);
    if (!ret)
        return NULL;

    // 将分配的内存清零
    memset(ret, 0, total_size);

    return ret;
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

  // Check if this is a page allocation (page-aligned pointer)
  if (((uint64)ptr & (PAGE_SIZE - 1)) == 0) {
    // This is a page allocation
    struct page *page = addr_to_page((paddr_t)ptr);
    if (!page) {
      return 0;
    }

    // Get the stored size from the mapping field
    size_t size = (size_t)page->kmalloc_size;

    return size;
  }

  // Small allocation with header
  struct kmalloc_header *header = ptr_to_header(ptr);

  // Verify magic value for corruption detection
  if (header->magic != KMALLOC_MAGIC) {
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

  // Get current size
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
  kprintf("Kernel memory allocator statistics:\n");

  // Print slab statistics
  slab_stats();

  // Print page statistics
  kprintf("Free page count: %d\n", get_free_page_count());
}

void* alloc_kernel_stack(){
	void* kstack = kmalloc(PAGE_SIZE);
  return kstack + PAGE_SIZE - 16;
}


/**
 * kstrdup - Duplicate a string with kmalloc
 * @s: The string to duplicate
 * @gfp: Memory allocation flags
 *
 * Allocates memory and copies the given string into it.
 * Returns the pointer to the new string or NULL on allocation failure.
 */
char *kstrdup(const char *s, uint32 gfp)
{
    size_t len;
    char *buf;
    
    if (!s)
        return NULL;
    
    len = strlen(s) + 1;
    buf = kmalloc(len);
    if (buf) {
        memcpy(buf, s, len);
    }
    return buf;
}

/**
 * kstrndup - Duplicate a string with kmalloc limiting the size
 * @s: The string to duplicate
 * @max: Maximum length to copy
 * @gfp: Memory allocation flags
 *
 * Allocates memory and copies up to @max characters from the given string.
 * The result is always null-terminated.
 * Returns the pointer to the new string or NULL on allocation failure.
 */
char *kstrndup(const char *s, size_t max, uint32 gfp)
{
    size_t len;
    char *buf;
    
    if (!s)
        return NULL;
    
    len = strlen(s);
    buf = kmalloc(len + 1);
    if (buf) {
        memcpy(buf, s, len);
        buf[len] = '\0';
    }
    return buf;
}
