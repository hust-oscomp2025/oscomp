#ifndef _PMM_H_
#define _PMM_H_

#include "config.h"
#include "util/types.h"

// Initialize phisical memeory manager
void pmm_init();
// Allocate a free phisical page
void* alloc_page();
void* Alloc_page();
// Free an allocated page
void free_page(void* pa);

void* kmalloc(size_t size);
void kfree(void* ptr);

extern int vm_alloc_stage[NCPU];

#endif