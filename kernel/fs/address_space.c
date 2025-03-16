#include <kernel/fs/address_space.h>
#include <kernel/mm/kmalloc.h>

#include <util/spinlock.h>
#include <util/atomic.h>
#include <kernel/types.h>
#include <string.h>

// 简单实现的radix树，用于存储页对象
#define RADIX_TREE_MAP_SHIFT 6  // 每个节点可以存储 2^6 = 64 个项
#define RADIX_TREE_MAP_SIZE (1UL << RADIX_TREE_MAP_SHIFT)
#define RADIX_TREE_MAP_MASK (RADIX_TREE_MAP_SIZE - 1)
#define RADIX_TREE_MAX_HEIGHT 4  // 支持到 2^24 个页

struct radix_tree_node {
    unsigned int height;          // 树的高度
    unsigned int count;           // 节点中的项数
    void *slots[RADIX_TREE_MAP_SIZE]; // 存储子节点或页对象
};

// 页缓存LRU链表
static struct list_head page_lru_list;
static spinlock_t lru_lock = SPINLOCK_INIT;
static uint64 lru_page_count = 0;
#define MAX_LRU_PAGES 1024  // 最大缓存页数





// 初始化地址空间子系统
void address_space_init(void) {
    INIT_LIST_HEAD(&page_lru_list);
}

// 创建radix树节点
static struct radix_tree_node *radix_tree_node_alloc(void) {
    struct radix_tree_node *node = (struct radix_tree_node *)kmalloc(sizeof(struct radix_tree_node));
    if (node) {
        memset(node, 0, sizeof(struct radix_tree_node));
        node->height = 1;
    }
    return node;
}

// 在radix树中查找页
static struct page *radix_tree_lookup(void *root, uint64 index) {
    struct radix_tree_node *node = (struct radix_tree_node *)root;
    void **slot;
    
    if (!node)
        return NULL;
    
    if (node->height == 1) {
        if (index >= RADIX_TREE_MAP_SIZE)
            return NULL;
        return (struct page *)node->slots[index & RADIX_TREE_MAP_MASK];
    }
    
    // 多层树的查找
    int height = node->height;
    while (height > 1) {
        unsigned int shift = (height - 1) * RADIX_TREE_MAP_SHIFT;
        unsigned int offset = (index >> shift) & RADIX_TREE_MAP_MASK;
        
        slot = &node->slots[offset];
        node = (struct radix_tree_node *)*slot;
        if (!node)
            return NULL;
        
        height--;
    }
    
    // 最后一层
    return (struct page *)node->slots[index & RADIX_TREE_MAP_MASK];
}

// 在radix树中插入页
static int radix_tree_insert(void **rootp, uint64 index, struct page *page) {
    struct radix_tree_node *node;
    void **slot;
    int error;
    
    // 如果树为空，创建根节点
    if (!*rootp) {
        *rootp = radix_tree_node_alloc();
        if (!*rootp)
            return -1;  // 内存分配失败
    }
    
    node = (struct radix_tree_node *)*rootp;
    
    // 检查树高度是否足够
    unsigned int max_index = 1UL << (node->height * RADIX_TREE_MAP_SHIFT);
    if (index >= max_index) {
        // 需要增加树高度
        // 这里简化处理，实际应该扩展树
        return -1;
    }
    
    // 单层树的简单插入
    if (node->height == 1) {
        node->slots[index & RADIX_TREE_MAP_MASK] = page;
        node->count++;
        return 0;
    }
    
    // 多层树的插入
    int height = node->height;
    while (height > 1) {
        unsigned int shift = (height - 1) * RADIX_TREE_MAP_SHIFT;
        unsigned int offset = (index >> shift) & RADIX_TREE_MAP_MASK;
        
        slot = &node->slots[offset];
        if (!*slot) {
            // 创建新节点
            *slot = radix_tree_node_alloc();
            if (!*slot)
                return -1;
            ((struct radix_tree_node *)*slot)->height = height - 1;
        }
        
        node = (struct radix_tree_node *)*slot;
        height--;
    }
    
    // 在最后一层插入页
    node->slots[index & RADIX_TREE_MAP_MASK] = page;
    node->count++;
    return 0;
}

// 从radix树中删除页
static struct page *radix_tree_delete(void **rootp, uint64 index) {
    struct radix_tree_node *node = (struct radix_tree_node *)*rootp;
    struct page *page = NULL;
    
    if (!node)
        return NULL;
    
    // 单层树的简单删除
    if (node->height == 1) {
        page = (struct page *)node->slots[index & RADIX_TREE_MAP_MASK];
        if (page) {
            node->slots[index & RADIX_TREE_MAP_MASK] = NULL;
            node->count--;
        }
        return page;
    }
    
    // 多层树的删除(简化实现，仅删除叶子节点)
    int height = node->height;
    struct radix_tree_node *path[RADIX_TREE_MAX_HEIGHT];
    unsigned int offsets[RADIX_TREE_MAX_HEIGHT];
    int level = 0;
    
    path[level] = node;
    while (level < height - 1) {
        unsigned int shift = (height - level - 1) * RADIX_TREE_MAP_SHIFT;
        unsigned int offset = (index >> shift) & RADIX_TREE_MAP_MASK;
        
        offsets[level] = offset;
        node = (struct radix_tree_node *)node->slots[offset];
        if (!node)
            return NULL;
        
        path[++level] = node;
    }
    
    // 获取页并从叶子节点中删除
    page = (struct page *)node->slots[index & RADIX_TREE_MAP_MASK];
    if (page) {
        node->slots[index & RADIX_TREE_MAP_MASK] = NULL;
        node->count--;
    }
    
    // 简化：不实现节点合并和树高度收缩
    return page;
}

// 创建新的address_space对象
struct address_space *address_space_create(struct inode *host, 
                                          const struct address_space_operations *a_ops) {
    struct address_space *mapping = (struct address_space *)kmalloc(sizeof(struct address_space));
    if (!mapping)
        return NULL;
    
    memset(mapping, 0, sizeof(struct address_space));
    mapping->host = host;
    mapping->a_ops = a_ops;
    atomic_set(&mapping->i_mmap_writable, 0);
		spinlock_init(&mapping->tree_lock);
    mapping->page_tree = NULL;
    mapping->nrpages = 0;
    
    return mapping;
}

// 释放address_space对象
void address_space_destroy(struct address_space *mapping) {
    if (!mapping)
        return;
    
    // 简化实现：应该释放所有缓存页
    invalidate_inode_pages(mapping);
    
    kfree(mapping);
}

// 从缓存中查找页
struct page *find_get_page(struct address_space *mapping, uint64 index) {
    struct page *page;
    
    spinlock_lock(&mapping->tree_lock);
    page = radix_tree_lookup(mapping->page_tree, index);
    if (page)
        get_page(page);
    spinlock_unlock(&mapping->tree_lock);
    
    return page;
}


// 从缓存中查找页，如果不存在则分配一个新页
struct page *find_or_create_page(struct address_space *mapping, uint64 index) {
    struct page *page = find_get_page(mapping, index);
    if (page)
        return page;
    
    // 分配新页
    page = alloc_page();
    if (!page)
        return NULL;
    
    init_page(page, mapping, index);
    
    // 添加到radix树
    spinlock_lock(&mapping->tree_lock);
    
    // 先检查是否已存在
    struct page *old_page = radix_tree_lookup(mapping->page_tree, index);
    if (old_page) {
        spinlock_unlock(&mapping->tree_lock);
        free_page_buffer(page->paddr);
        kfree(page);
        get_page(old_page);
        return old_page;
    }
    
    // 插入新页
    if (radix_tree_insert(&mapping->page_tree, index, page) != 0) {
        spinlock_unlock(&mapping->tree_lock);
        free_page_buffer(page->paddr);
        kfree(page);
        return NULL;
    }
    
    mapping->nrpages++;
    
    // 添加到LRU链表
    spinlock_lock(&lru_lock);
    list_add(&page->lru, &page_lru_list);
    lru_page_count++;
    
    // 如果LRU链表太长，回收最旧的页
    if (lru_page_count > MAX_LRU_PAGES) {
        struct list_head *lru_entry = page_lru_list.prev;
        struct page *lru_page = (struct page *)((char *)lru_entry - offsetof(struct page, lru));
        
        // 如果页可以被回收（引用计数为1且不是脏页）
        if (atomic_read(&lru_page->_refcount) == 1 && !(lru_page->flags & PAGE_DIRTY)) {
            list_del(&lru_page->lru);
            lru_page_count--;
            
            // 从radix树中删除
            struct address_space *lru_mapping = lru_page->mapping;
            if (lru_mapping) {
                spinlock_unlock(&lru_lock);  // 避免死锁
                spinlock_lock(&lru_mapping->tree_lock);
                radix_tree_delete(&lru_mapping->page_tree, lru_page->index);
                lru_mapping->nrpages--;
                spinlock_unlock(&lru_mapping->tree_lock);
                spinlock_lock(&lru_lock);
            }
            
            // 释放页
            free_page_buffer(lru_page->paddr);
            kfree(lru_page);
        }
    }
    
    spinlock_unlock(&lru_lock);
    spinlock_unlock(&mapping->tree_lock);
    
    return page;
}

// 初始化页结构体
void init_page(struct page *page, struct address_space *mapping, uint64 index) {
    page->index = index;
    page->mapping = mapping;
    atomic_set(&page->_refcount, 1);
    page->flags = 0;
}

// 将页写回存储设备
int write_page(struct page *page) {
    if (!page || !page->mapping || !page->mapping->a_ops || !page->mapping->a_ops->writepage)
        return -1;
    
    return page->mapping->a_ops->writepage(page, NULL);
}

// 将数据从用户空间复制到页
ssize_t copy_to_page(struct page *page, const char *buf, size_t count, loff_t offset) {
    if (!page || !page->paddr || offset >= PAGE_SIZE)
        return -1;
    
    // 确保不超出页大小
    if (offset + count > PAGE_SIZE)
        count = PAGE_SIZE - offset;
    
    // 复制数据
    memcpy((char *)page->paddr + offset, buf, count);
    
    // 标记页为脏
    set_page_dirty(page);
    
    return count;
}

// 将数据从页复制到用户空间
ssize_t copy_from_page(struct page *page, char *buf, size_t count, loff_t offset) {
    if (!page || !page->paddr || offset >= PAGE_SIZE)
        return -1;
    
    // 确保不超出页大小
    if (offset + count > PAGE_SIZE)
        count = PAGE_SIZE - offset;
    
    // 复制数据
    memcpy(buf, (char *)page->paddr + offset, count);
    
    return count;
}

// 将address_space中的所有脏页写回
int write_inode_pages(struct address_space *mapping) {
    if (!mapping || !mapping->a_ops || !mapping->a_ops->writepages)
        return mapping->a_ops->writepages(mapping, NULL);
    
    // 简单实现：遍历所有页并写回脏页
    // 这里应该使用更高效的方法，比如使用writepages批量写入
    // 这是一个简化版本
    spinlock_lock(&mapping->tree_lock);
    
    // 简化：只处理单层radix树
    struct radix_tree_node *root = (struct radix_tree_node *)mapping->page_tree;
    if (!root) {
        spinlock_unlock(&mapping->tree_lock);
        return 0;
    }
    
    int i;
    for (i = 0; i < RADIX_TREE_MAP_SIZE; i++) {
        struct page *page = (struct page *)root->slots[i];
        if (page && (page->flags & PAGE_DIRTY)) {
            get_page(page);  // 增加引用计数，防止页被释放
            spinlock_unlock(&mapping->tree_lock);
            
            write_page(page);
            
            spinlock_lock(&mapping->tree_lock);
            put_page(page);  // 减少引用计数
        }
    }
    
    spinlock_unlock(&mapping->tree_lock);
    return 0;
}

// 清除address_space中的页缓存
void invalidate_inode_pages(struct address_space *mapping) {
    if (!mapping)
        return;
    
    spinlock_lock(&mapping->tree_lock);
    
    // 简化：只处理单层radix树
    struct radix_tree_node *root = (struct radix_tree_node *)mapping->page_tree;
    if (!root) {
        spinlock_unlock(&mapping->tree_lock);
        return;
    }
    
    int i;
    for (i = 0; i < RADIX_TREE_MAP_SIZE; i++) {
        struct page *page = (struct page *)root->slots[i];
        if (page) {
            // 从radix树中删除
            root->slots[i] = NULL;
            root->count--;
            mapping->nrpages--;
            
            // 从LRU链表中删除
            spinlock_lock(&lru_lock);
            if (page->lru.next) {  // 如果页在链表中
                list_del(&page->lru);
                lru_page_count--;
            }
            spinlock_unlock(&lru_lock);
            
            // 如果有释放页的回调函数，调用它
            if (mapping->a_ops && mapping->a_ops->releasepage)
                mapping->a_ops->releasepage(page);
            
            // 释放页
            free_page_buffer(page->paddr);
            kfree(page);
        }
    }
    
    // 释放根节点
    kfree(root);
    mapping->page_tree = NULL;
    
    spinlock_unlock(&mapping->tree_lock);
}

// 释放页缓存使用的物理页
void free_page_buffer(paddr_t addr) {
	put_page((addr_to_page(addr)));
}