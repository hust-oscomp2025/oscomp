#ifndef _PMM_H_
#define _PMM_H_

#include <kernel/config.h>
#include <kernel/types.h>
#include <kernel/page.h>  // 引入页管理相关函数

// Initialize physical memory manager
void pmm_init();

// Allocate a free physical page without associated page struct
void* alloc_page();

// Allocate a free physical page with higher-level wrapper
void* Alloc_page();

// Free an allocated physical page
void free_page(void* pa);

// Kernel malloc and free
void* kmalloc(size_t size);
void kfree(void* ptr);


#endif