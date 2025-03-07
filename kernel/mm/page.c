#include <kernel/page.h>
#include <kernel/atomic.h>
#include <kernel/spinlock.h>
#include <kernel/list.h>
#include <util/string.h>
#include "spike_interface/spike_utils.h"
#include <kernel/sync_utils.h>

// 页结构数组，用于跟踪所有物理页
static struct page *page_map = NULL;
static uint64 total_pages = 0;
static uint64 page_map_size = 0;

// LRU页链表头
static LIST_HEAD(page_lru_list);
static spinlock_t page_lru_lock = SPINLOCK_INIT;

// 物理内存布局
static uint64 mem_base_addr;
static uint64 mem_size;

// 空闲页链表相关
typedef struct node {
  struct node *next;
} list_node;

static list_node free_page_list = { NULL };
volatile static int counter = 1;

// 获取页框号 (PFN)
static unsigned long get_pfn(void *addr) {
  return ((uint64)addr - mem_base_addr) / PGSIZE;
}

// 初始化页结构
void init_page_struct(struct page* page) {
  if (!page)
    return;
  
  page->flags = 0;
  atomic_set(&page->_refcount, 0);
  page->index = 0;
  page->virtual_address = NULL;
  page->mapping = NULL;
  INIT_LIST_HEAD(&page->lru);
  atomic_flag_clear(&page->page_lock.lock);
}

// 初始化页管理子系统
void page_init(uint64 mem_base, uint64 mem_size_bytes, uint64 start_addr) {
  // 保存物理内存布局信息
  mem_base_addr = mem_base;
  mem_size = mem_size_bytes;
  
  // 计算总页数
  total_pages = mem_size_bytes / PGSIZE;
  page_map_size = total_pages * sizeof(struct page);
  
  // 为页结构数组分配空间
  uint64 page_map_pages = (page_map_size + PGSIZE - 1) / PGSIZE;
  
  // 保留空间给页结构数组
  page_map = (struct page *)start_addr;
  
  // 初始化所有页结构
  for (uint64 i = 0; i < total_pages; i++) {
    init_page_struct(&page_map[i]);
  }
  
  sprint("Page subsystem initialized: %d pages, map size: %lx bytes at 0x%lx\n", 
         total_pages, page_map_size, (uint64)page_map);
  
  // 返回空闲内存开始地址（页结构后的地址）
  uint64 free_start = start_addr + (page_map_pages * PGSIZE);
  sprint("Free memory starts at: 0x%lx\n", free_start);
}

// 获取一个空闲物理页，不设置page结构
void* get_free_page(void) {
  list_node *n = free_page_list.next;
  if (n) {
    free_page_list.next = n->next;
  }
  return (void *)n;
}

// 释放一个物理页到空闲列表，不涉及page结构
void put_free_page(void* addr) {
  // 检查地址是否在有效范围内
  if (((uint64)addr % PGSIZE) != 0 || 
      (uint64)addr < mem_base_addr || 
      (uint64)addr >= mem_base_addr + mem_size) {
    panic("put_free_page: invalid address 0x%lx\n", addr);
  }

  // 插入物理页到空闲链表
  list_node *n = (list_node *)addr;
  n->next = free_page_list.next;
  free_page_list.next = n;
}

// 根据页框号获取页结构
struct page* pfn_to_page(unsigned long pfn) {
  if (pfn >= total_pages)
    return NULL;
  return &page_map[pfn];
}

// 根据物理地址获取页结构
struct page* virt_to_page(void *addr) {
  unsigned long pfn = get_pfn(addr);
  return pfn_to_page(pfn);
}

// 根据页结构获取页框号
unsigned long page_to_pfn(struct page* page) {
  if (!page)
    return 0;
  return page - page_map;
}

// 根据页结构获取物理地址
void* page_to_virt(struct page* page) {
  if (!page)
    return NULL;
  unsigned long pfn = page_to_pfn(page);
  return (void*)(mem_base_addr + pfn * PGSIZE);
}

// 分配单个页结构及对应物理页
struct page* page_alloc(void) {
  void* pa = get_free_page();
  if (!pa)
    return NULL;
  
  memset(pa, 0, PGSIZE);
  struct page* page = virt_to_page(pa);
  if (page) {
    init_page_struct(page);
    atomic_set(&page->_refcount, 1);  // 初始引用计数为1
    page->virtual_address = pa;
  }
  
  return page;
}

// 释放单个页结构及对应物理页
void page_free(struct page* page) {
  if (!page)
    return;
  
  // 确保引用计数为0
  if (atomic_read(&page->_refcount) != 0) {
    // 引用计数不为0，不能释放
    return;
  }
  
  void* pa = page_to_virt(page);
  if (pa) {
    init_page_struct(page);  // 重置页结构
    put_free_page(pa);       // 释放物理页
  }
}

// 分配多个连续页，返回第一个页的page结构
// 目前简单实现，未实现真正的buddy系统
struct page* alloc_pages(int order) {
  if (order < 0)
    return NULL;
    
  if (order == 0)
    return page_alloc();  // 单页分配
  
  // 目前不支持多页连续分配，可以在后续扩展中实现buddy系统
  return NULL;
}

// 释放多个连续页
// 目前简单实现，未实现真正的buddy系统
void free_pages(struct page* page, int order) {
  if (!page)
    return;
  
  if (order == 0) {
    page_free(page);  // 单页释放
    return;
  }
  
  // 目前不支持多页连续释放，可以在后续扩展中实现buddy系统
}

// 增加页引用计数
void get_page(struct page* page) {
  if (!page)
    return;
  atomic_inc(&page->_refcount);
}

// 减少页引用计数，如果降为0则返回1
int put_page(struct page* page) {
  if (!page)
    return 0;
  
  if (atomic_dec_and_test(&page->_refcount)) {
    // 引用计数降为0，页可以被释放
    return 1;
  }
  return 0;
}

// 设置页为脏
void set_page_dirty(struct page* page) {
  if (!page)
    return;
  page->flags |= PAGE_DIRTY;
}

// 清除页脏标志
void clear_page_dirty(struct page* page) {
  if (!page)
    return;
  page->flags &= ~PAGE_DIRTY;
}

// 测试页是否为脏
int test_page_dirty(struct page* page) {
  if (!page)
    return 0;
  return (page->flags & PAGE_DIRTY) != 0;
}

// 锁定页
void lock_page(struct page* page) {
  if (!page)
    return;
  spinlock_lock(&page->page_lock);
  page->flags |= PAGE_LOCKED;
}

// 解锁页
void unlock_page(struct page* page) {
  if (!page)
    return;
  page->flags &= ~PAGE_LOCKED;
  spinlock_unlock(&page->page_lock);
}

// 尝试锁定页
int trylock_page(struct page* page) {
  if (!page)
    return 0;
  
  if (spinlock_trylock(&page->page_lock)) {
    page->flags |= PAGE_LOCKED;
    return 1;
  }
  return 0;
}

// 清零页内容
void zero_page(struct page* page) {
  if (!page)
    return;
    
  void* addr = page_to_virt(page);
  if (addr) {
    memset(addr, 0, PGSIZE);
  }
}

// 复制页内容
void copy_page(struct page* dest, struct page* src) {
  if (!dest || !src)
    return;
    
  void* dest_addr = page_to_virt(dest);
  void* src_addr = page_to_virt(src);
  
  if (dest_addr && src_addr) {
    memcpy(dest_addr, src_addr, PGSIZE);
  }
}