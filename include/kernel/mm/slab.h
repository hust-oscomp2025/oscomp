/**
 * @file slab.h
 * @brief Slab allocator for kernel small memory allocations
 * 
 * This file defines the slab allocator interface, which is used by
 * kmalloc for small memory allocations (up to 4KB).
 */

 #ifndef _SLAB_H
 #define _SLAB_H
 
 #include <kernel/types.h>
 #include <kernel/mm/page.h>
 #include <util/spinlock.h>
 #include <util/list.h>
 
 /**
	* @brief Slab header structure - manages a single slab
	*/
 struct slab_header {
		 struct list_head list;      // List node
		 struct page *page;          // Physical page
		 unsigned int free_count;    // Number of free objects
		 unsigned int total_count;   // Total number of objects
		 unsigned int obj_size;      // Object size
		 unsigned char bitmap[0];    // Bitmap marking object usage
 };
 
 /**
	* @brief Slab cache structure - manages objects of a specific size
	*/
 struct kmem_cache {
		 spinlock_t lock;            // Cache lock
		 size_t obj_size;            // Size of objects in this cache
		 struct list_head slabs_full;    // Fully allocated slabs
		 struct list_head slabs_partial;  // Partially allocated slabs
		 struct list_head slabs_free;     // Empty slabs
		 unsigned int free_objects;  // Total number of free objects
 };
 
 /**
	* @brief Initialize the slab allocator
	*/
 void slab_init(void);
 
 /**
	* @brief Allocate an object from a slab cache
	* 
	* @param size Size of object to allocate
	* @return void* Pointer to allocated object, or NULL if failed
	*/
 void *slab_alloc(size_t size);
 
 /**
	* @brief Free a slab-allocated object
	* 
	* @param ptr Pointer to object
	* @return int 1 if object was from slab cache, 0 otherwise
	*/
 int slab_free(void *ptr);
 
 /**
	* @brief Get cache for a specific size
	* 
	* @param size Object size
	* @return struct kmem_cache* Appropriate cache or NULL if too large
	*/
 struct kmem_cache *slab_cache_for_size(size_t size);
 
 /**
	* @brief Print slab allocator statistics
	*/
 void slab_stats(void);
 
 #endif /* _SLAB_H */