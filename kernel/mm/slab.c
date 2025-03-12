/**
 * @file slab.c
 * @brief Slab allocator implementation for small memory allocations
 * 
 * Implements a slab allocator for efficient small memory allocation.
 * Used by kmalloc for allocations up to 4KB in size.
 */

 #include <kernel/mm/slab.h>
 #include <kernel/mm/page.h>
 #include <util/atomic.h>
 #include <util/spinlock.h>
 #include <util/list.h>
 #include <util/string.h>
 #include <spike_interface/spike_utils.h>
 
 // Slab sizes including allocation headers (8 bytes)
 // These sizes are the total allocation size
 #define SLAB_SIZES_COUNT 8
 const size_t slab_sizes[SLAB_SIZES_COUNT] = {
		 16,    // 8 bytes kernel data + 8 byte header
		 32,    // 24 bytes kernel data + 8 byte header
		 64,    // 56 bytes kernel data + 8 byte header
		 128,   // 120 bytes kernel data + 8 byte header
		 256,   // 248 bytes kernel data + 8 byte header
		 512,   // 504 bytes kernel data + 8 byte header
		 1024,  // 1016 bytes kernel data + 8 byte header
		 2048   // 2040 bytes kernel data + 8 byte header
 };
 
 // Global array of slab caches
 static struct kmem_cache slab_caches[SLAB_SIZES_COUNT];
 
 /* Bitmap operations */
 
 /**
	* @brief Set a bit in bitmap
	*/
 static inline void set_bit(unsigned char *bitmap, unsigned int idx) {
		 bitmap[idx / 8] |= (1 << (idx % 8));
 }
 
 /**
	* @brief Clear a bit in bitmap
	*/
 static inline void clear_bit(unsigned char *bitmap, unsigned int idx) {
		 bitmap[idx / 8] &= ~(1 << (idx % 8));
 }
 
 /**
	* @brief Test a bit in bitmap
	*/
 static inline int test_bit(unsigned char *bitmap, unsigned int idx) {
		 return (bitmap[idx / 8] >> (idx % 8)) & 1;
 }
 
 /**
	* @brief Find first zero bit in bitmap
	*/
 static int find_first_zero(unsigned char *bitmap, unsigned int size) {
		 for (unsigned int i = 0; i < (size + 7) / 8; i++) {
				 for (unsigned int j = 0; j < 8; j++) {
						 if (!(bitmap[i] & (1 << j)) && (i * 8 + j < size)) {
								 return i * 8 + j;
						 }
				 }
		 }
		 return -1;  // No free objects
 }
 
 /**
	* @brief Get object index in slab
	*/
 static inline unsigned int obj_index(struct slab_header *slab, void *obj) {
		 return ((char *)obj - (char *)(slab + 1)) / slab->obj_size;
 }
 
 /**
	* @brief Get object address from index
	*/
 static inline void *index_to_obj(struct slab_header *slab, unsigned int idx) {
		 return (void *)((char *)(slab + 1) + idx * slab->obj_size);
 }
 
 /**
	* @brief Initialize a new slab
	*/
 static struct slab_header *slab_header_init(size_t obj_size) {
		 // Allocate a physical page
		 struct page *page = alloc_page();
		 if (!page) return NULL;
 
		 // Use beginning of page for slab header
		 struct slab_header *slab = page_to_virt(page);
		 
		 // Calculate bitmap size and number of objects
		 // Make sure objects are aligned to 8 bytes
		 obj_size = (obj_size + 7) & ~7;
		 
		 unsigned int bitmap_size = sizeof(unsigned char) * ((PAGE_SIZE / obj_size + 7) / 8);
		 unsigned int usable_size = PAGE_SIZE - sizeof(struct slab_header) - bitmap_size;
		 unsigned int total_objs = usable_size / obj_size;
		 
		 // Initialize slab header
		 INIT_LIST_HEAD(&slab->list);
		 slab->page = page;
		 slab->free_count = total_objs;
		 slab->total_count = total_objs;
		 slab->obj_size = obj_size;
		 
		 // Clear bitmap (0 = free)
		 memset(slab->bitmap, 0, bitmap_size);
		 
		 return slab;
 }
 
 /**
	* @brief Allocate object from slab
	*/
 static void *slab_alloc_obj(struct kmem_cache *cache) {
		 // Check for partial slabs
		 if (list_empty(&cache->slabs_partial)) {
				 // If no partial slabs, check free slabs
				 if (list_empty(&cache->slabs_free)) {
						 // Need to create a new slab
						 struct slab_header *slab = slab_header_init(cache->obj_size);
						 if (!slab) return NULL;  // Out of memory
						 
						 // Add new slab to partial list
						 list_add(&slab->list, &cache->slabs_partial);
				 } else {
						 // Use first free slab
						 struct list_head *first = cache->slabs_free.next;
						 list_del(first);
						 list_add(first, &cache->slabs_partial);
				 }
		 }
		 
		 // Get first slab from partial list
		 struct slab_header *slab = list_entry(cache->slabs_partial.next, struct slab_header, list);
		 
		 // Find first free object
		 int idx = find_first_zero(slab->bitmap, slab->total_count);
		 if (idx < 0) {
				 // This shouldn't happen, as partial slabs should have free objects
				 panic("slab_alloc_obj: no free object in partial slab\n");
		 }
		 
		 // Mark as used
		 set_bit(slab->bitmap, idx);
		 slab->free_count--;
		 cache->free_objects--;
		 
		 // If slab is now full, move to full list
		 if (slab->free_count == 0) {
				 list_del(&slab->list);
				 list_add(&slab->list, &cache->slabs_full);
		 }
		 
		 // Get pointer to the allocated object
		 void *obj = index_to_obj(slab, idx);
		 
		 // Clear the object memory
		 memset(obj, 0, slab->obj_size);
		 
		 return obj;
 }
 
 /**
	* @brief Free slab object
	*/
 static void slab_free_obj(struct kmem_cache *cache, struct slab_header *slab, void *obj) {
		 // Get object index
		 unsigned int idx = obj_index(slab, obj);
		 
		 // Check index validity
		 if (idx >= slab->total_count) {
				 panic("slab_free_obj: invalid object index\n");
		 }
		 
		 // Check if object is already free
		 if (!test_bit(slab->bitmap, idx)) {
				 panic("slab_free_obj: double free detected\n");
		 }
		 
		 // Mark as free
		 clear_bit(slab->bitmap, idx);
		 slab->free_count++;
		 cache->free_objects++;
		 
		 // Update slab state
		 if (slab->free_count == 1) {
				 // Move from full to partial
				 list_del(&slab->list);
				 list_add(&slab->list, &cache->slabs_partial);
		 } else if (slab->free_count == slab->total_count) {
				 // Move from partial to free
				 list_del(&slab->list);
				 list_add(&slab->list, &cache->slabs_free);
				 
				 // If too many free objects, consider releasing some slabs
				 // Simple approach: if more than 2 free slabs, release one
				 if (cache->free_objects > 2 * slab->total_count) {
						 // Remove last free slab
						 struct list_head *last = cache->slabs_free.prev;
						 list_del(last);
						 
						 struct slab_header *free_slab = list_entry(last, struct slab_header, list);
						 cache->free_objects -= free_slab->free_count;
						 
						 // Free the page
						 struct page *page = free_slab->page;
						 free_page(page);
				 }
		 }
 }
 
 /**
	* @brief Initialize slab allocator
	*/
 void slab_init(void) {
		 // Initialize all slab caches
		 for (int i = 0; i < SLAB_SIZES_COUNT; i++) {
				 struct kmem_cache *cache = &slab_caches[i];
				 
				 spinlock_init(&cache->lock);
				 cache->obj_size = slab_sizes[i];
				 INIT_LIST_HEAD(&cache->slabs_full);
				 INIT_LIST_HEAD(&cache->slabs_partial);
				 INIT_LIST_HEAD(&cache->slabs_free);
				 cache->free_objects = 0;
				 
				 sprint("  Initialized slab cache for size %d bytes\n", cache->obj_size);
		 }
 }
 
 /**
	* @brief Get cache for a specific size
	*/
 struct kmem_cache *slab_cache_for_size(size_t size) {
		 // Round up size to next multiple of 8 bytes for alignment
		 size = (size + 7) & ~7;
		 
		 for (int i = 0; i < SLAB_SIZES_COUNT; i++) {
				 if (size <= slab_sizes[i]) {
						 return &slab_caches[i];
				 }
		 }
		 
		 // If we get here, the size is too large for any slab
		 // This shouldn't happen as callers should check the size
		 return NULL;
 }
 
 /**
	* @brief Allocate object from slab allocator
	*/
 void *slab_alloc(size_t size) {
		 // Verify size is within our slab allocator range
		 if (size > 2048) {
				 return NULL;  // Too large for slab allocator
		 }
		 
		 struct kmem_cache *cache = slab_cache_for_size(size);
		 if (!cache) {
				 // Should not happen if size <= 2048
				 panic("slab_alloc: failed to find cache for size %d\n", size);
		 }
		 
		 spinlock_lock(&cache->lock);
		 void *ptr = slab_alloc_obj(cache);
		 spinlock_unlock(&cache->lock);
		 
		 return ptr;
 }
 
 /**
	* @brief Find the slab containing an object
	*/
 static struct slab_header *find_slab(void *ptr) {
		 // Convert to page-aligned address to find slab header
		 uintptr_t addr = (uintptr_t)ptr & ~(PAGE_SIZE - 1);
		 return (struct slab_header *)addr;
 }
 
 /**
	* @brief Find and free slab object
	*/
 int slab_free(void *ptr) {
		 if (!ptr) return 0;
		 
		 // Find slab containing this object
		 struct slab_header *slab = find_slab(ptr);
		 
		 // Verify this is a valid slab object
		 unsigned int idx = obj_index(slab, ptr);
		 if (idx >= slab->total_count) {
				 return 0;  // Not a valid slab object
		 }
		 
		 // Find matching cache
		 for (int i = 0; i < SLAB_SIZES_COUNT; i++) {
				 struct kmem_cache *cache = &slab_caches[i];
				 
				 if (cache->obj_size == slab->obj_size) {
						 spinlock_lock(&cache->lock);
						 slab_free_obj(cache, slab, ptr);
						 spinlock_unlock(&cache->lock);
						 return 1;  // Successfully freed
				 }
		 }
		 
		 return 0;  // Not a slab object
 }
 
 /**
	* @brief Print slab allocator statistics
	*/
 void slab_stats(void) {
		 sprint("Slab caches:\n");
		 for (int i = 0; i < SLAB_SIZES_COUNT; i++) {
				 struct kmem_cache *cache = &slab_caches[i];
				 
				 spinlock_lock(&cache->lock);
				 
				 int full_count = 0, partial_count = 0, free_count = 0;
				 struct list_head *pos;
				 
				 list_for_each(pos, &cache->slabs_full) {
						 full_count++;
				 }
				 
				 list_for_each(pos, &cache->slabs_partial) {
						 partial_count++;
				 }
				 
				 list_for_each(pos, &cache->slabs_free) {
						 free_count++;
				 }
				 
				 sprint("  Size %4d bytes: %2d full, %2d partial, %2d free, %4d free objects\n",
								 cache->obj_size, full_count, partial_count, free_count, cache->free_objects);
				 
				 spinlock_unlock(&cache->lock);
		 }
 }