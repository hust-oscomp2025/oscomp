/**
 * @file kmalloc.h
 * @brief Kernel memory allocation interface
 * 
 * Provides kmalloc/kfree interfaces for kernel dynamic memory allocation.
 * This implementation uses a slab allocator for small allocations and
 * page allocator for large allocations.
 */

 #ifndef _KMALLOC_H
 #define _KMALLOC_H
 #include <kernel/types.h>
 
 /**
	* @brief Initialize kernel memory allocation subsystem
	*/
 void kmem_init(void);
 
 /**
	* @brief Allocate kernel memory
	* 
	* @param size Size in bytes to allocate
	* @return void* Pointer to allocated memory, or NULL if failed
	*/
 void *kmalloc(size_t size);
 
 /**
	* @brief Free previously allocated kernel memory
	* 
	* @param ptr Pointer to memory allocated by kmalloc
	*/
 void kfree(void *ptr);
 
 /**
	* @brief Allocate kernel memory and zero it
	* 
	* @param size Size in bytes to allocate
	* @return void* Pointer to allocated and zeroed memory, or NULL if failed
	*/
 void *kzalloc(size_t size);
 
 /**
	* @brief Resize an allocated memory block
	* 
	* @param ptr Pointer to memory previously allocated by kmalloc, or NULL
	* @param new_size New size in bytes
	* @return void* Pointer to the resized memory block, or NULL if failed
	*/
 void *krealloc(void *ptr, size_t new_size);
 
 /**
	* @brief Get the allocated size of a memory block
	* 
	* @param ptr Pointer to memory allocated by kmalloc
	* @return size_t Size of the allocation in bytes
	*/
 size_t ksize(void *ptr);
 
 /**
	* @brief Print memory allocator statistics
	*/
 void kmalloc_stats(void);



 void* alloc_kernel_stack(void);
 
 #endif /* _KMALLOC_H */