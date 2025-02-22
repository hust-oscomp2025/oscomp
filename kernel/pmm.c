#include "pmm.h"
#include "util/functions.h"
#include "riscv.h"
#include "config.h"
#include "util/string.h"
#include "memlayout.h"
#include "spike_interface/spike_utils.h"
#include "sync_utils.h"
#include "vmm.h"
#include "global.h"

// _end is defined in kernel/kernel.lds, it marks the ending (virtual) address of PKE kernel
extern char _end[];
// g_mem_size is defined in spike_interface/spike_memory.c, it indicates the size of our
// (emulated) spike machine. g_mem_size's value is obtained when initializing HTIF. 
extern uint64 g_mem_size;

static uint64 free_mem_start_addr;  //beginning address of free memory
static uint64 free_mem_end_addr;    //end address of free memory (not included)

int vm_alloc_stage[NCPU] = { 0 }; // 0 for kernel alloc, 1 for user alloc
typedef struct node {
  struct node *next;
} list_node;

// g_free_mem_list is the head of the list of free physical memory pages
static list_node g_free_mem_list;

//
// actually creates the freepage list. each page occupies 4KB (PGSIZE), i.e., small page.
// PGSIZE is defined in kernel/riscv.h, ROUNDUP is defined in util/functions.h.
//
static void create_freepage_list(uint64 start, uint64 end) {
  g_free_mem_list.next = 0;
  for (uint64 p = ROUNDUP(start, PGSIZE); p + PGSIZE < end; p += PGSIZE)
    free_page( (void *)p );
}

//
// place a physical page at *pa to the free list of g_free_mem_list (to reclaim the page)
//
void free_page(void *pa) {
  if (((uint64)pa % PGSIZE) != 0 || (uint64)pa < free_mem_start_addr || (uint64)pa >= free_mem_end_addr)
    panic("free_page 0x%lx \n", pa);

  // insert a physical page to g_free_mem_list
  list_node *n = (list_node *)pa;
  n->next = g_free_mem_list.next;
  g_free_mem_list.next = n;
}

void kheap_insert(heap_block* prev, heap_block* newblock){
  newblock->next = prev->next;
  newblock->prev = prev;
  if(prev->next != NULL){
    prev->next->prev = newblock;
  }
  prev->next = newblock;
}

void kheap_alloc(){
  heap_block* new_page = Alloc_page();
  new_page->size = PGSIZE - sizeof(heap_block);
  new_page->free = 1;
  kheap_insert(&kernel_heap_head, new_page);
}


void* kmalloc(size_t size){
  int required_size = ALIGN(size + sizeof(heap_block),8);
  //目前只服务大小小于一个页的内核堆分配请求。因为内核堆难以稳定获取连续的物理内存。
  if(size <= 0 || required_size > PGSIZE){
    return NULL;
  }
  int hartid = read_tp();
  heap_block* iterator = &kernel_heap_head;
  // 遍历内核堆表，找一个大小符合要求的块，进行分割
  while(iterator->next){
      sprint("iterator->next=%x\n",iterator->next);
      sprint("iterator->next->size=%x\n",iterator->next->size);
    if(iterator->next->free && iterator->next->size >= required_size){
      iterator->next->free = 0;
      // 可以分割
      if(iterator->next->size > required_size + sizeof(heap_block)){
        
        heap_block* new_block = (heap_block *)((uintptr_t)iterator->next + required_size);
        new_block->size = iterator->next->size - required_size - sizeof(heap_block);
        new_block->free = 1;
        iterator->next->size = required_size;
        kheap_insert(iterator->next,new_block); 
        
      }
      //sprint("sizeof(heap_block)=%x\n",sizeof(heap_block));
      sprint("iterator->next=%x\n",iterator->next);
      sprint("iterator->next->size=%x\n",iterator->next->size);
      return (void*)((uint64)iterator->next + sizeof(heap_block));
    }
    iterator = iterator->next;
  }
  // 没找到符合要求的内存块，在内核堆表头之后插入一个新的页。
  kheap_alloc();
  // 重新分配。
  return kmalloc(size);
}



void kfree(void* ptr){
  if(ptr == NULL){
    return;
  }
  heap_block* block = (heap_block*)((uintptr_t)ptr - sizeof(heap_block));
  block->free = 1;
  // 合并同一页上前一个内存块
  if(block->prev && block->prev->free == 1 && ((uint64)block) % PGSIZE == ((uint64)block->prev) % PGSIZE){
    block->prev->size += sizeof(heap_block) + block->size;
    block->prev->next = block->next;
    if(block->next){
      block->next->prev = block->prev;
    }
    block = block->prev;
  }
  // 合并同一页上后一个内存块
  if(block->next && block->next->free == 1 && ((uint64)block) % PGSIZE == ((uint64)block->next) % PGSIZE){
    block->size += sizeof(heap_block) + block->next->size;
    block->next = block->next->next;
    if(block->next->next){
      block->next->next->prev = block;
    }
  }
  // 如果得到了一个空页，还给内存池。
  if(block->size == PGSIZE - sizeof(heap_block)){
    block->prev->next = block->next;
    if(block->next){
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
void *alloc_page(void) {
  if(NCPU > 1)
    sync_barrier(&counter,NCPU);
  list_node *n = g_free_mem_list.next;
  uint64 hartid = 0;
  if (vm_alloc_stage[hartid]) {
    sprint("hartid = %ld: alloc page 0x%x\n", hartid, n);
  }
  if (n) g_free_mem_list.next = n->next;
  return (void *)n;
}

void* Alloc_page(void){
  void *pa = alloc_page();
  if (pa == 0)
    panic("uvmalloc mem alloc failed\n");
  memset((void *)pa, 0, PGSIZE);
  return pa;
}

//
// pmm_init() establishes the list of free physical pages according to available
// physical memory space.
//
void pmm_init() {
  // start of kernel program segment
  uint64 g_kernel_start = KERN_BASE;
  uint64 g_kernel_end = (uint64)&_end;

  uint64 pke_kernel_size = g_kernel_end - g_kernel_start;
  sprint("PKE kernel start 0x%lx, PKE kernel end: 0x%lx, PKE kernel size: 0x%lx .\n",
    g_kernel_start, g_kernel_end, pke_kernel_size);

  // free memory starts from the end of PKE kernel and must be page-aligined
  free_mem_start_addr = ROUNDUP(g_kernel_end , PGSIZE);

  // recompute g_mem_size to limit the physical memory space that our riscv-pke kernel
  // needs to manage
  g_mem_size = MIN(PKE_MAX_ALLOWABLE_RAM, g_mem_size);
  if( g_mem_size < pke_kernel_size )
    panic( "Error when recomputing physical memory size (g_mem_size).\n" );

  free_mem_end_addr = g_mem_size + DRAM_BASE;
  sprint("free physical memory address: [0x%lx, 0x%lx] \n", free_mem_start_addr,
    free_mem_end_addr - 1);

  sprint("kernel memory manager is initializing ...\n");
  // create the list of free pages
  create_freepage_list(free_mem_start_addr, free_mem_end_addr);
}
