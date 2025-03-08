#ifndef _PAGE_H
#define _PAGE_H

#include <kernel/types.h>
#include <kernel/list.h>
#include <kernel/atomic.h>
#include <kernel/spinlock.h>
#include <kernel/riscv.h>

// Forward declarations
struct address_space;

/**
 * 物理页结构体 - Linux风格的页描述符
 */
struct page {
    uint64 flags;                // 页标志
    atomic_t _refcount;          // 引用计数
    uint64 index;                // 在映射文件中的页索引
    void *virtual_address;       // 页在内核空间中的虚拟地址
    struct address_space *mapping; // 所属的address_space
    struct list_head lru;        // LRU链表节点
    spinlock_t page_lock;        // 页锁，用于同步访问
};

// 页标志位定义
#define PAGE_DIRTY     (1UL << 0)  // 脏页，需要回写
#define PAGE_UPTODATE  (1UL << 1)  // 页内容是最新的
#define PAGE_LOCKED    (1UL << 2)  // 页已锁定，不可回收
#define PAGE_SLAB      (1UL << 3)  // 页用于slab分配器
#define PAGE_BUDDY     (1UL << 4)  // 页用于buddy系统
#define PAGE_RESERVED  (1UL << 5)  // 页已被保留，不可分配

// 初始化页管理子系统
void page_init(uint64 mem_base, uint64 mem_size, uint64 start_addr);

// 页分配与释放
struct page* alloc_pages(int order);          // 分配2^order个连续页
void free_pages(struct page* page, int order); // 释放2^order个连续页
struct page* page_alloc(void);                // 分配单个页并返回page结构
void page_free(struct page* page);            // 释放单个页

// 页框号与地址转换函数
struct page* pfn_to_page(unsigned long pfn);
struct page* virt_to_page(void* addr);
unsigned long page_to_pfn(struct page* page);
void* page_to_virt(struct page* page);

// 页引用计数操作
void get_page(struct page* page);           // 增加页引用计数
int put_page(struct page* page);            // 减少页引用计数

// 页标志位操作
void set_page_dirty(struct page* page);     // 设置页为脏
void clear_page_dirty(struct page* page);   // 清除页脏标志
int test_page_dirty(struct page* page);     // 测试页是否为脏

// 页锁操作
void lock_page(struct page* page);          // 锁定页
void unlock_page(struct page* page);        // 解锁页
int trylock_page(struct page* page);        // 尝试锁定页

// 页内容操作
void zero_page(struct page* page);          // 清零页内容
void copy_page(struct page* dest, struct page* src); // 复制页内容

// 检查页内容是否最新
static inline int page_uptodate(struct page *page) {
	return (page->flags & PAGE_UPTODATE) != 0;
}

// 将页内容标记为最新
static inline void set_page_uptodate(struct page *page) {
	page->flags |= PAGE_UPTODATE;
}

int get_free_page_count(void);


// 内部使用函数，但需要在头文件中声明以便PMM调用
void init_page_struct(struct page* page);
void* get_free_page(void);            // 获取一个空闲物理页，不设置page结构
void put_free_page(void* addr);       // 释放一个物理页到空闲列表，不涉及page结构

#endif /* _PAGE_H */