#ifndef _ADDRESS_SPACE_H
#define _ADDRESS_SPACE_H

#include <kernel/types.h>
#include <kernel/atomic.h>
#include <kernel/spinlock.h>
#include <kernel/vmm.h>
#include <kernel/riscv.h>
#include <kernel/list.h>
#include <kernel/page.h>

// 前向声明，避免循环引用
struct inode;

/**
 * 地址空间对象，代表一个文件的页缓存集合
 */
struct address_space {
    struct inode *host;                // 拥有此地址空间的inode
    atomic_t i_mmap_writable;          // 可写映射计数
    spinlock_t tree_lock;              // 保护radix树的锁
    void *page_tree;                   // 径向树根节点，存储page对象
    uint64 nrpages;                    // 缓存的页数
    const struct address_space_operations *a_ops; // 地址空间操作
};

/**
 * 地址空间操作方法，类似文件操作
 */
struct address_space_operations {
    int (*writepage)(struct page *page, void *wbc);
    int (*readpage)(struct address_space *mapping, struct page *page);
    int (*writepages)(struct address_space *mapping, void *wbc);
    int (*readpages)(struct address_space *mapping, struct list_head *pages, unsigned nr_pages);
    int (*set_page_dirty)(struct page *page);
    int (*releasepage)(struct page *page);
    void (*invalidatepage)(struct page *page, unsigned int offset, unsigned int length);
};

// 初始化地址空间子系统
void address_space_init(void);

// 创建新的address_space对象
struct address_space *address_space_create(struct inode *host, 
                                          const struct address_space_operations *a_ops);

// 释放address_space对象
void address_space_destroy(struct address_space *mapping);

// 从缓存中查找页，如果不存在则返回NULL
struct page *find_get_page(struct address_space *mapping, uint64 index);

// 从缓存中查找页，如果不存在则分配一个新页
struct page *find_or_create_page(struct address_space *mapping, uint64 index);

void init_page(struct page *page, struct address_space *mapping, uint64 index);

// 将页写回存储设备
int write_page(struct page *page);


// 锁定页，防止其被回收
void lock_page(struct page *page);

// 解锁页
void unlock_page(struct page *page);

// 将数据从用户空间复制到页
ssize_t copy_to_page(struct page *page, const char *buf, size_t count, loff_t offset);

// 将数据从页复制到用户空间
ssize_t copy_from_page(struct page *page, char *buf, size_t count, loff_t offset);

// 将address_space中的所有脏页写回
int write_inode_pages(struct address_space *mapping);

// 清除address_space中的页缓存
void invalidate_inode_pages(struct address_space *mapping);

// 分配物理页并返回虚拟地址
void *alloc_page_buffer(void);

// 释放页缓存使用的物理页
void free_page_buffer(void *addr);

#endif /* _ADDRESS_SPACE_H */