#include <kernel/pmm.h>
#include <kernel/types.h>
#include <kernel/riscv.h>
#include <kernel/config.h>
#include <util/string.h>
#include <kernel/memlayout.h>
#include <spike_interface/spike_utils.h>
#include <kernel/sync_utils.h>
#include <kernel/vmm.h>
#include <kernel/list.h>
#include <kernel/page.h>


// _end is defined in kernel/kernel.lds, it marks the ending (virtual) address of PKE kernel
extern char _end[];
// g_mem_size is defined in spike_interface/spike_memory.c, it indicates the size of our
// (emulated) spike machine. g_mem_size's value is obtained when initializing HTIF. 
extern uint64 g_mem_size;

static uint64 free_mem_start_addr;  //beginning address of free memory
static uint64 free_mem_end_addr;    //end address of free memory (not included)


//内核堆的虚拟头节点
heap_block kernel_heap_head;


void kheap_insert(heap_block* prev, heap_block* newblock){
  newblock->next = prev->next;
  newblock->prev = prev;
  if(prev->next != NULL){
    prev->next->prev = newblock;
  }
  prev->next = newblock;
}

void kheap_alloc() {
  heap_block* new_page = Alloc_page();
  new_page->size = PGSIZE - sizeof(heap_block);
  new_page->free = 1;
  kheap_insert(&kernel_heap_head, new_page);
}

void* kmalloc(size_t size) {
  int required_size = ALIGN(size + sizeof(heap_block), 8);
  // 目前只服务大小小于一个页的内核堆分配请求。因为内核堆难以稳定获取连续的物理内存。
  if (size <= 0 || required_size > PGSIZE) {
    return NULL;
  }
  int hartid = read_tp();
  heap_block* iterator = &kernel_heap_head;
  // 遍历内核堆表，找一个大小符合要求的块，进行分割
  while (iterator->next) {
    if (iterator->next->free && iterator->next->size >= required_size) {
      iterator->next->free = 0;
      // 可以分割
      if (iterator->next->size > required_size + sizeof(heap_block)) {
        heap_block* new_block = (heap_block *)((uintptr_t)iterator->next + required_size);
        new_block->size = iterator->next->size - required_size - sizeof(heap_block);
        new_block->free = 1;
        iterator->next->size = required_size;
        kheap_insert(iterator->next, new_block); 
      }
      return (void*)((uint64)iterator->next + sizeof(heap_block));
    }
    iterator = iterator->next;
  }
  // 没找到符合要求的内存块，在内核堆表头之后插入一个新的页。
  kheap_alloc();
  // 重新分配。
  return kmalloc(size);
}


void kfree(void* ptr) {
  if (ptr == NULL) {
    return;
  }
  heap_block* block = (heap_block*)((uintptr_t)ptr - sizeof(heap_block));
  block->free = 1;
  // 合并同一页上前一个内存块
  if (block->prev && block->prev->free == 1 && ((uint64)block) % PGSIZE == ((uint64)block->prev) % PGSIZE) {
    block->prev->size += sizeof(heap_block) + block->size;
    block->prev->next = block->next;
    if (block->next) {
      block->next->prev = block->prev;
    }
    block = block->prev;
  }
  // 合并同一页上后一个内存块
  if (block->next && block->next->free == 1 && ((uint64)block) % PGSIZE == ((uint64)block->next) % PGSIZE) {
    block->size += sizeof(heap_block) + block->next->size;
    block->next = block->next->next;
    if (block->next->next) {
      block->next->next->prev = block;
    }
  }
  // 如果得到了一个空页，还给内存池。
  if (block->size == PGSIZE - sizeof(heap_block)) {
    block->prev->next = block->next;
    if (block->next) {
      block->next->prev = block->prev;
    }
    free_page(block);
  }
  return;
}


//
// takes the first free page from g_free_mem_list, and returns (allocates) it.
// Allocates only ONE page!
//
volatile static int counter = 1;
// 从空闲链表分配一个物理页
void *alloc_page(void) {
  return get_free_page();  // 使用page.c中定义的函数
}



// 包装分配函数，确保返回已清零的页
void* Alloc_page(void) {
  void *pa = alloc_page();
  if (pa == 0)
    panic("uvmalloc mem alloc failed\n");
  memset((void *)pa, 0, PGSIZE);
  return pa;
}

// 释放物理页
void free_page(void *pa) {
  put_free_page(pa);  // 使用page.c中定义的函数
}



// PMM初始化
void pmm_init() {
  // 内核程序段起止地址
  uint64 g_kernel_start = KERN_BASE;
  uint64 g_kernel_end = (uint64)&_end;

  uint64 pke_kernel_size = g_kernel_end - g_kernel_start;
  sprint("PKE kernel start 0x%lx, PKE kernel end: 0x%lx, PKE kernel size: 0x%lx.\n",
    g_kernel_start, g_kernel_end, pke_kernel_size);

  // 空闲内存起始地址必须页对齐
  free_mem_start_addr = ROUNDUP(g_kernel_end, PGSIZE);

  // 重新计算g_mem_size以限制物理内存空间
  g_mem_size = MIN(PKE_MAX_ALLOWABLE_RAM, g_mem_size);
  if (g_mem_size < pke_kernel_size)
    panic("Error when recomputing physical memory size (g_mem_size).\n");

  free_mem_end_addr = DRAM_BASE + g_mem_size;
  
  sprint("free physical memory address: [0x%lx, 0x%lx] \n", free_mem_start_addr,
    free_mem_end_addr - 1);

  sprint("kernel memory manager is initializing ...\n");
  
  // 初始化页管理子系统
  page_init(DRAM_BASE, g_mem_size, free_mem_start_addr);
  
  // 更新空闲内存起始地址（考虑页结构数组的空间）
  // 这个值由page_init内部更新，需要page_init实现返回更新后的值
  // 目前简单处理，不改变原值
  uint64 page_map_size = (g_mem_size / PGSIZE) * sizeof(struct page);
  uint64 page_map_pages = (page_map_size + PGSIZE - 1) / PGSIZE;
  free_mem_start_addr += page_map_pages * PGSIZE;
  
  // 创建空闲页链表
  for (uint64 p = ROUNDUP(free_mem_start_addr, PGSIZE); p + PGSIZE < free_mem_end_addr; p += PGSIZE)
    put_free_page((void *)p);

  // 初始化内核堆
  kernel_heap_head.next = NULL;
  kernel_heap_head.prev = NULL;
  kernel_heap_head.free = 0;
  kernel_heap_head.size = 0;
  
  sprint("Physical memory manager initialization complete.\n");
}