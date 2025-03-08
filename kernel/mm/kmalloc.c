/**
 * @file kmalloc.c
 * @brief Kernel memory allocation implementation
 * 
 * This file implements the kernel memory allocation interface using
 * slab allocator for small allocations and page allocator for large
 * allocations.
 */

 #include <kernel/kmalloc.h>
 #include <kernel/list.h>
 #include <kernel/slab.h>
 #include <kernel/page.h>
 #include <kernel/atomic.h>
 #include <kernel/spinlock.h>
 #include <util/string.h>
 #include <spike_interface/spike_utils.h>
 

 /*
	* Large memory allocation tracker - used for allocations > 4KB
	*/
 struct large_allocation {
		 void *addr;                // Allocated address
		 size_t size;               // Allocation size
		 unsigned int pages;        // Number of pages
		 struct large_allocation *next;  // Next node in linked list
 };
 
 // Head of large allocation tracking list
 static struct large_allocation *large_allocations = NULL;
 static spinlock_t large_alloc_lock = SPINLOCK_INIT;
 
 /**
	* @brief Record a large memory allocation
	*/
 static void record_large_alloc(void *addr, size_t size, unsigned int pages) {
		 struct large_allocation *alloc = (struct large_allocation *)kmalloc(sizeof(struct large_allocation));
		 if (!alloc) {
				 panic("record_large_alloc: failed to allocate memory for tracking\n");
		 }
		 
		 alloc->addr = addr;
		 alloc->size = size;
		 alloc->pages = pages;
		 
		 spinlock_lock(&large_alloc_lock);
		 alloc->next = large_allocations;
		 large_allocations = alloc;
		 spinlock_unlock(&large_alloc_lock);
 }
 
 /**
	* @brief Find and remove a large allocation record
	*/
 static struct large_allocation *find_and_remove_large_alloc(void *ptr) {
		 spinlock_lock(&large_alloc_lock);
		 
		 struct large_allocation *curr = large_allocations;
		 struct large_allocation *prev = NULL;
		 
		 while (curr != NULL) {
				 if (curr->addr == ptr) {
						 // Remove from list
						 if (prev == NULL) {
								 large_allocations = curr->next;
						 } else {
								 prev->next = curr->next;
						 }
						 
						 spinlock_unlock(&large_alloc_lock);
						 return curr;
				 }
				 
				 prev = curr;
				 curr = curr->next;
		 }
		 
		 spinlock_unlock(&large_alloc_lock);
		 return NULL;  // Not found
 }
 
 /**
	* @brief Initialize kernel memory allocator
	*/
 void kmem_init(void) {
		 sprint("Initializing kernel memory allocator...\n");
		 
		 // Initialize large allocation tracker
		 spinlock_init(&large_alloc_lock);
		 large_allocations = NULL;
		 
		 // Initialize slab allocator
		 slab_init();
		 
		 sprint("Kernel memory allocator initialized\n");
 }
 
 /**
	* @brief Allocate kernel memory
	*/
 void *kmalloc(size_t size) {
		 if (size == 0) return NULL;
		 
		 // For small allocations (≤ 4KB), use slab allocator
		 if (size <= 4096) {
				 return slab_alloc(size);
		 }
		 
		 // For large allocations (> 4KB), use page allocator directly
		 // Calculate required pages
		 unsigned int pages = (size + PGSIZE - 1) / PGSIZE;
		 struct page *page = alloc_pages(0);  // Currently only supports single page allocation
		 
		 if (!page) {
				 return NULL;  // Out of memory
		 }
		 
		 void *addr = page_to_virt(page);
		 if (!addr) {
				 return NULL;
		 }
		 
		 // Record this large allocation
		 record_large_alloc(addr, size, pages);
		 
		 return addr;
 }
 
 /**
	* @brief Free kernel memory
	*/
 void kfree(void *ptr) {
		 if (!ptr) return;
		 
		 // First try to free as a slab object
		 if (slab_free(ptr)) {
				 return;  // Successfully freed slab object
		 }
		 
		 // Try to free as a large allocation
		 struct large_allocation *alloc = find_and_remove_large_alloc(ptr);
		 if (alloc) {
				 // Free the pages
				 struct page *page = virt_to_page(ptr);
				 if (page) {
						 page_free(page);
				 }
				 
				 // Free the tracking structure
				 // Note: This will recursively call kfree, but for a small object
				 kfree(alloc);
				 return;
		 }
		 
		 // If we get here, the pointer is invalid
		 panic("kfree: invalid pointer 0x%lx\n", (uint64)ptr);
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
	* @brief Resize allocated memory
	*/
 void *krealloc(void *ptr, size_t new_size) {
		 if (!ptr) return kmalloc(new_size);
		 if (new_size == 0) {
				 kfree(ptr);
				 return NULL;
		 }
		 
		 // Try to find original allocation size
		 size_t old_size = 0;
		 
		 // Check if it's a large allocation
		 spinlock_lock(&large_alloc_lock);
		 struct large_allocation *curr = large_allocations;
		 while (curr != NULL) {
				 if (curr->addr == ptr) {
						 old_size = curr->size;
						 break;
				 }
				 curr = curr->next;
		 }
		 spinlock_unlock(&large_alloc_lock);
		 
		 // If not a large allocation, estimate size from slab caches
		 if (old_size == 0) {
				 // For slab objects, we don't know exact size, but can estimate upper bound
				 for (int i = 0; i < SLAB_SIZES_COUNT; i++) {
						 // Assume it belongs to the first possible cache we find
						 old_size = slab_sizes[i];
						 break;
				 }
		 }
		 
		 // If new size is smaller or equal, return the same pointer
		 if (new_size <= old_size) {
				 return ptr;
		 }
		 
		 // Allocate new block
		 void *new_ptr = kmalloc(new_size);
		 if (!new_ptr) return NULL;
		 
		 // Copy data
		 memcpy(new_ptr, ptr, old_size);
		 
		 // Free old block
		 kfree(ptr);
		 
		 return new_ptr;
 }
 
 /**
	* @brief Get size of allocated memory block
	*/
 size_t ksize(void *ptr) {
		 if (!ptr) return 0;
		 
		 // Check large allocations first
		 spinlock_lock(&large_alloc_lock);
		 struct large_allocation *curr = large_allocations;
		 while (curr != NULL) {
				 if (curr->addr == ptr) {
						 size_t size = curr->size;
						 spinlock_unlock(&large_alloc_lock);
						 return size;
				 }
				 curr = curr->next;
		 }
		 spinlock_unlock(&large_alloc_lock);
		 
		 // Not a large allocation, delegate to slab subsystem
		 struct kmem_cache *cache = slab_cache_for_size(0); // Just to access the cache array
		 if (!cache) {
				 panic("ksize: slab caches not initialized\n");
		 }
		 
		 // Search through all slab caches
		 for (int i = 0; i < SLAB_SIZES_COUNT; i++) {
				 cache = &((struct kmem_cache*)cache)[i]; // Access cache array
				 
				 spinlock_lock(&cache->lock);
				 
				 // Check all slabs
				 int found = 0;
				 struct list_head *pos;
				 
				 // Check full slabs
				 list_for_each(pos, &cache->slabs_full) {
						 struct slab_header *slab = list_entry(pos, struct slab_header, list);
						 void *slab_start = (void*)(slab + 1);
						 void *slab_end = (char*)slab_start + 
								 (slab->total_count * slab->obj_size);
						 
						 if (ptr >= slab_start && ptr < slab_end) {
								 found = 1;
								 break;
						 }
				 }
				 
				 // Check partial slabs
				 if (!found) {
						 list_for_each(pos, &cache->slabs_partial) {
								 struct slab_header *slab = list_entry(pos, struct slab_header, list);
								 void *slab_start = (void*)(slab + 1);
								 void *slab_end = (char*)slab_start + 
										 (slab->total_count * slab->obj_size);
								 
								 if (ptr >= slab_start && ptr < slab_end) {
										 found = 1;
										 break;
								 }
						 }
				 }
				 
				 spinlock_unlock(&cache->lock);
				 
				 if (found) {
						 return cache->obj_size;
				 }
		 }
		 
		 // Cannot determine size
		 panic("ksize: invalid pointer 0x%lx\n", (uint64)ptr);
		 return 0;
 }
 
 /**
	* @brief Print memory allocator statistics
	*/
 void kmalloc_stats(void) {
		 sprint("Kernel memory allocator statistics:\n");
		 
		 // Print slab statistics
		 slab_stats();
		 
		 // Print large allocation statistics
		 spinlock_lock(&large_alloc_lock);
		 
		 int large_count = 0;
		 size_t large_total = 0;
		 struct large_allocation *curr = large_allocations;
		 
		 while (curr != NULL) {
				 large_count++;
				 large_total += curr->size;
				 curr = curr->next;
		 }
		 
		 sprint("Large allocations: %d blocks, %lu bytes total\n", 
					 large_count, large_total);
		 
		 spinlock_unlock(&large_alloc_lock);
		 
		 // Print page statistics
		 sprint("Free page count: %d\n", get_free_page_count());
 }