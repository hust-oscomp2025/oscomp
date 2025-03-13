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
 #include <kernel/mm/pagetable.h>
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
	 uint32_t magic; // Magic value to detect corruption
 };
 
 // Magic value to detect corruption
 #define KMALLOC_MAGIC 0xA110C8ED
 #define HEADER_SIZE sizeof(struct kmalloc_header)
 
 static spinlock_t kmalloc_lock = SPINLOCK_INIT;
 
// 在kernel初始化中被调用
 void kmem_init(void) {
	 sprint("Initializing kernel memory allocator...\n");
 
	 // Initialize spinlock
	 spinlock_init(&kmalloc_lock);
 
	 slab_init();
	 pagetable_server_init();


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
	sprint("kmalloc: start\n");
	 if (size == 0)
		 return NULL;
 
	 void *mem = NULL;
 
	 // Round up to multiple of 8 bytes for alignment
	 size = (size + 7) & ~7;
 
	 // Calculate total size including header
	 size_t total_size = size + HEADER_SIZE;
 
	 // For small allocations (up to 2048 bytes), use slab allocator
	 if (total_size <= 2048) {
		 struct kmalloc_header *header = slab_alloc(total_size);
		 if (header) {
			 header->size = size;
			 header->magic = KMALLOC_MAGIC;
			 mem = header_to_ptr(header);
		 }
	 } else {
		 // For large allocations, use page allocator without headers
		 unsigned int npages = ROUNDUP(size,PAGE_SIZE) / PAGE_SIZE;
		 struct page *page = alloc_pages(npages); // Convert order to pages
		 
		 if (page) {
			 // Store the page count in the page's index field
			 page->index = npages;
			 // Store the actual requested size in the page's mapping field
			 page->mapping = (void*)size;
			 mem = page_to_virt(page);
		 }
	 }
	 sprint("kmalloc: end\n");
 
	 return mem;
 }
 
 /**
	* @brief Free kernel memory
	*/
 void kfree(void *ptr) {
	 if (!ptr)
		 return;
	 
	 sprint("calling kfree with ptr=%lx\n", ptr);
	 
	 // Check if this is a page allocation (page-aligned pointer)
	 if (((uint64)ptr & (PAGE_SIZE - 1)) == 0) {
		 // This is a page allocation
		 struct page *page = virt_to_page(ptr);
		 if (!page) {
			 panic("kfree: invalid pointer 0x%lx\n", (uint64)ptr);
		 }
		 
		 // Get the number of pages from the index field
		 unsigned int pages = page->index;
		 if (pages == 0) {
			 panic("kfree: invalid page count for pointer 0x%lx\n", (uint64)ptr);
		 }
		 
		 // Free the pages
		 free_pages(page, pages - 1);
		 return;
	 }
	 
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
		 struct page *page = virt_to_page(ptr);
		 if (!page) {
			 return 0;
		 }
		 
		 // Get the stored size from the mapping field
		 size_t size = (size_t)page->mapping;
		 
		 // If stored size is 0, fallback to calculating from page count
		 if (size == 0) {
			 size = page->index * PAGE_SIZE;
		 }
		 
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
	 sprint("Kernel memory allocator statistics:\n");
 
	 // Print slab statistics
	 slab_stats();
 
	 // Print page statistics
	 sprint("Free page count: %d\n", get_free_page_count());
 }

void* alloc_kernel_stack(){
	void* kstack = kmalloc(PAGE_SIZE);
	return kstack + PAGE_SIZE - 16;
}