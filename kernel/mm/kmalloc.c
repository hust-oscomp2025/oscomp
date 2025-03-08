#include <kernel/page.h>
#include <kernel/atomic.h>
#include <kernel/spinlock.h>
#include <kernel/list.h>
#include <util/string.h>
#include <spike_interface/spike_utils.h>

/*
 * 内核内存分配系统 - 统一的kmalloc/kfree接口
 * 
 * 特点：
 * 1. 小块内存(<= 4KB)使用slab分配器，减少碎片和提高性能
 * 2. 大块内存(> 4KB)直接使用页分配器，按页对齐
 */

// Slab分配器的缓存大小类别（字节）
#define SLAB_SIZES_COUNT 10
static const size_t slab_sizes[SLAB_SIZES_COUNT] = {
    16, 32, 64, 128, 256, 512, 1024, 2048, 3072, 4096
};

// Slab头部结构
struct slab_header {
    struct list_head list;      // 链表节点
    struct page *page;          // 对应的物理页
    unsigned int free_count;    // 空闲对象数量
    unsigned int total_count;   // 总对象数量 
    unsigned int obj_size;      // 对象大小
    unsigned char bitmap[0];    // 位图，标记对象使用情况
};

// 每种大小的slab缓存
struct kmem_cache {
    spinlock_t lock;            // 保护该缓存的锁
    size_t obj_size;            // 对象大小
    struct list_head slabs_full;    // 完全使用的slabs
    struct list_head slabs_partial;  // 部分使用的slabs
    struct list_head slabs_free;     // 完全空闲的slabs
    unsigned int free_objects;  // 所有slab中的空闲对象总数
};

// 全局slab缓存数组
static struct kmem_cache slab_caches[SLAB_SIZES_COUNT];

// 大块内存页映射表 - 跟踪直接通过页分配器分配的内存
struct large_allocation {
    void *addr;                // 分配的地址
    size_t size;               // 分配的大小
    unsigned int pages;        // 分配的页数
    struct large_allocation *next;  // 链表下一个节点
};

// 大块内存分配链表头
static struct large_allocation *large_allocations = NULL;
static spinlock_t large_alloc_lock = SPINLOCK_INIT;

// 魔数用于验证内存块有效性
#define KMEM_MAGIC 0xABCDEF12

// 获取对象在slab中的索引
static inline unsigned int obj_index(struct slab_header *slab, void *obj) {
    return ((char *)obj - (char *)(slab + 1)) / slab->obj_size;
}

// 获取对象在slab中的地址
static inline void *index_to_obj(struct slab_header *slab, unsigned int idx) {
    return (void *)((char *)(slab + 1) + idx * slab->obj_size);
}

// Bitmap操作函数
static inline void set_bit(unsigned char *bitmap, unsigned int idx) {
    bitmap[idx / 8] |= (1 << (idx % 8));
}

static inline void clear_bit(unsigned char *bitmap, unsigned int idx) {
    bitmap[idx / 8] &= ~(1 << (idx % 8));
}

static inline int test_bit(unsigned char *bitmap, unsigned int idx) {
    return (bitmap[idx / 8] >> (idx % 8)) & 1;
}

// 查找第一个设置为0的位（空闲对象）
static int find_first_zero(unsigned char *bitmap, unsigned int size) {
    for (unsigned int i = 0; i < size; i++) {
        for (unsigned int j = 0; j < 8; j++) {
            if (!(bitmap[i] & (1 << j)) && (i * 8 + j < size)) {
                return i * 8 + j;
            }
        }
    }
    return -1;  // 没有找到空闲对象
}

// 初始化slab
static struct slab_header *slab_init(size_t obj_size) {
    // 分配一个物理页
    struct page *page = page_alloc();
    if (!page) return NULL;

    // 页的开始部分用于slab头
    struct slab_header *slab = page_to_virt(page);
    
    // 计算位图大小和可用对象数量
    unsigned int bitmap_size = sizeof(unsigned char) * ((PGSIZE / obj_size + 7) / 8);
    unsigned int usable_size = PGSIZE - sizeof(struct slab_header) - bitmap_size;
    unsigned int total_objs = usable_size / obj_size;
    
    // 初始化slab头
    INIT_LIST_HEAD(&slab->list);
    slab->page = page;
    slab->free_count = total_objs;
    slab->total_count = total_objs;
    slab->obj_size = obj_size;
    
    // 清空位图 (0表示空闲)
    memset(slab->bitmap, 0, bitmap_size);
    
    return slab;
}

// 从slab分配一个对象
static void *slab_alloc_obj(struct kmem_cache *cache) {
    // 检查部分使用的slabs
    if (list_empty(&cache->slabs_partial)) {
        // 如果没有部分使用的slab，检查空闲slabs
        if (list_empty(&cache->slabs_free)) {
            // 需要创建新的slab
            struct slab_header *slab = slab_init(cache->obj_size);
            if (!slab) return NULL;  // 内存不足
            
            // 将新slab加入到部分使用链表
            list_add(&slab->list, &cache->slabs_partial);
        } else {
            // 使用第一个空闲slab
            struct list_head *first = cache->slabs_free.next;
            list_del(first);
            list_add(first, &cache->slabs_partial);
        }
    }
    
    // 从部分使用链表中取第一个slab
    struct slab_header *slab = list_entry(cache->slabs_partial.next, struct slab_header, list);
    
    // 找到第一个空闲对象
    int idx = find_first_zero(slab->bitmap, slab->total_count);
    if (idx < 0) {
        // 这不应该发生，因为部分使用的slab应该有空闲对象
        panic("slab_alloc_obj: no free object in partial slab\n");
    }
    
    // 标记为已使用
    set_bit(slab->bitmap, idx);
    slab->free_count--;
    cache->free_objects--;
    
    // 如果slab已满，移至full链表
    if (slab->free_count == 0) {
        list_del(&slab->list);
        list_add(&slab->list, &cache->slabs_full);
    }
    
    return index_to_obj(slab, idx);
}

// 释放一个slab对象
static void slab_free_obj(struct kmem_cache *cache, struct slab_header *slab, void *obj) {
    // 获取对象索引
    unsigned int idx = obj_index(slab, obj);
    
    // 检查索引有效性
    if (idx >= slab->total_count) {
        panic("slab_free_obj: invalid object index\n");
    }
    
    // 检查对象是否已经被释放
    if (!test_bit(slab->bitmap, idx)) {
        panic("slab_free_obj: double free detected\n");
    }
    
    // 标记为空闲
    clear_bit(slab->bitmap, idx);
    slab->free_count++;
    cache->free_objects++;
    
    // 更新slab状态
    if (slab->free_count == 1) {
        // 从full移至partial
        list_del(&slab->list);
        list_add(&slab->list, &cache->slabs_partial);
    } else if (slab->free_count == slab->total_count) {
        // 从partial移至free
        list_del(&slab->list);
        list_add(&slab->list, &cache->slabs_free);
        
        // 如果空闲对象太多，考虑释放一些slab
        // 这里简单化：如果有超过2个空闲slab，释放一个
        if (cache->free_objects > 2 * slab->total_count) {
            // 删除最后一个空闲slab
            struct list_head *last = cache->slabs_free.prev;
            list_del(last);
            
            struct slab_header *free_slab = list_entry(last, struct slab_header, list);
            cache->free_objects -= free_slab->free_count;
            
            // 释放页
            struct page *page = free_slab->page;
            page_free(page);
        }
    }
}

// 查找并释放slab对象
static int find_and_free_slab_obj(void *ptr) {
    // 遍历所有缓存，查找包含该对象的slab
    for (int i = 0; i < SLAB_SIZES_COUNT; i++) {
        struct kmem_cache *cache = &slab_caches[i];
        
        spinlock_lock(&cache->lock);
        
        // 检查完全使用的slabs
        struct list_head *pos;
        list_for_each(pos, &cache->slabs_full) {
            struct slab_header *slab = list_entry(pos, struct slab_header, list);
            void *slab_start = slab + 1;  // 数据区开始
            void *slab_end = (char *)slab_start + slab->total_count * slab->obj_size;
            
            if (ptr >= slab_start && ptr < slab_end) {
                // 找到了对象
                slab_free_obj(cache, slab, ptr);
                spinlock_unlock(&cache->lock);
                return 1;  // 成功释放
            }
        }
        
        // 检查部分使用的slabs
        list_for_each(pos, &cache->slabs_partial) {
            struct slab_header *slab = list_entry(pos, struct slab_header, list);
            void *slab_start = slab + 1;
            void *slab_end = (char *)slab_start + slab->total_count * slab->obj_size;
            
            if (ptr >= slab_start && ptr < slab_end) {
                slab_free_obj(cache, slab, ptr);
                spinlock_unlock(&cache->lock);
                return 1;
            }
        }
        
        spinlock_unlock(&cache->lock);
    }
    
    return 0;  // 不是slab对象
}

// 记录大块内存分配
static void record_large_alloc(void *addr, size_t size, unsigned int pages) {
    struct large_allocation *alloc = (struct large_allocation *)kmalloc(sizeof(struct large_allocation));
    if (!alloc) {
        panic("record_large_alloc: failed to allocate memory for tracking\n");
    }
    
    alloc->addr = addr;
    alloc->size = size;
    alloc->pages = pages;
    
    spinlock_lock(&large_alloc_lock);
    alloc->next = large_allocations;
    large_allocations = alloc;
    spinlock_unlock(&large_alloc_lock);
}

// 查找并删除大块内存记录
static struct large_allocation *find_and_remove_large_alloc(void *ptr) {
    spinlock_lock(&large_alloc_lock);
    
    struct large_allocation *curr = large_allocations;
    struct large_allocation *prev = NULL;
    
    while (curr != NULL) {
        if (curr->addr == ptr) {
            // 从链表中移除
            if (prev == NULL) {
                large_allocations = curr->next;
            } else {
                prev->next = curr->next;
            }
            
            spinlock_unlock(&large_alloc_lock);
            return curr;
        }
        
        prev = curr;
        curr = curr->next;
    }
    
    spinlock_unlock(&large_alloc_lock);
    return NULL;  // 未找到
}

// 根据大小选择合适的缓存
static struct kmem_cache *get_cache_for_size(size_t size) {
    for (int i = 0; i < SLAB_SIZES_COUNT; i++) {
        if (size <= slab_sizes[i]) {
            return &slab_caches[i];
        }
    }
    return NULL;  // 大小超过所有缓存，需要页分配
}

// 初始化内存分配系统
void kmem_init(void) {
    sprint("Initializing kernel memory allocator...\n");
    
    // 初始化大内存分配锁
    spinlock_init(&large_alloc_lock);
    large_allocations = NULL;
    
    // 初始化所有slab缓存
    for (int i = 0; i < SLAB_SIZES_COUNT; i++) {
        struct kmem_cache *cache = &slab_caches[i];
        
        spinlock_init(&cache->lock);
        cache->obj_size = slab_sizes[i];
        INIT_LIST_HEAD(&cache->slabs_full);
        INIT_LIST_HEAD(&cache->slabs_partial);
        INIT_LIST_HEAD(&cache->slabs_free);
        cache->free_objects = 0;
        
        sprint("  Initialized slab cache for size %d bytes\n", cache->obj_size);
    }
    
    sprint("Kernel memory allocator initialized\n");
}

// 分配指定大小的内存
void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    
    // 1. 对小块内存使用slab分配器
    if (size <= 4096) {
        struct kmem_cache *cache = get_cache_for_size(size);
        if (!cache) {
            // 不应该发生，因为我们有最大4KB的缓存
            panic("kmalloc: no suitable cache for size %lu\n", size);
        }
        
        spinlock_lock(&cache->lock);
        void *ptr = slab_alloc_obj(cache);
        spinlock_unlock(&cache->lock);
        
        return ptr;
    }
    
    // 2. 对大块内存直接使用页分配器
    // 计算需要的页数
    unsigned int pages = (size + PGSIZE - 1) / PGSIZE;
    struct page *page = alloc_pages(0);  // 目前只支持单页分配
    
    if (!page) {
        return NULL;  // 内存不足
    }
    
    void *addr = page_to_virt(page);
    if (!addr) {
        return NULL;
    }
    
    // 记录这个大块内存分配
    record_large_alloc(addr, size, pages);
    
    return addr;
}

// 释放之前分配的内存
void kfree(void *ptr) {
    if (!ptr) return;
    
    // 1. 首先尝试作为slab对象释放
    if (find_and_free_slab_obj(ptr)) {
        return;  // 成功释放slab对象
    }
    
    // 2. 尝试作为大块内存释放
    struct large_allocation *alloc = find_and_remove_large_alloc(ptr);
    if (alloc) {
        // 释放页
        struct page *page = virt_to_page(ptr);
        if (page) {
            page_free(page);
        }
        
        // 释放跟踪结构
        // 注：这里会递归调用kmalloc，但因为结构体很小，会走slab路径
        kfree(alloc);
        return;
    }
    
    // 如果到这里，说明指针无效
    panic("kfree: invalid pointer 0x%lx\n", (uint64)ptr);
}

// 分配指定大小的内存并清零
void *kzalloc(size_t size) {
    void *ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

// 重新分配内存块大小
void *krealloc(void *ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }
    
    // 尝试找到原始分配的大小
    size_t old_size = 0;
    
    // 检查是否是大块内存
    spinlock_lock(&large_alloc_lock);
    struct large_allocation *curr = large_allocations;
    while (curr != NULL) {
        if (curr->addr == ptr) {
            old_size = curr->size;
            break;
        }
        curr = curr->next;
    }
    spinlock_unlock(&large_alloc_lock);
    
    // 如果不是大块内存，假设它是一个slab对象
    if (old_size == 0) {
        // 对于slab对象，我们不知道它的确切大小，但可以估计上限
        for (int i = 0; i < SLAB_SIZES_COUNT; i++) {
            // 假设它属于找到的第一个可能的缓存
            // 这是一个粗略估计，可能不准确
            old_size = slab_sizes[i];
            break;
        }
    }
    
    // 如果新大小小于等于当前估计大小，直接返回
    if (new_size <= old_size) {
        return ptr;
    }
    
    // 分配新块
    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) return NULL;
    
    // 复制数据
    memcpy(new_ptr, ptr, old_size);
    
    // 释放旧块
    kfree(ptr);
    
    return new_ptr;
}

// 获取分配块的实际大小（估计值）
size_t ksize(void *ptr) {
    if (!ptr) return 0;
    
    // 检查是否是大块内存
    spinlock_lock(&large_alloc_lock);
    struct large_allocation *curr = large_allocations;
    while (curr != NULL) {
        if (curr->addr == ptr) {
            size_t size = curr->size;
            spinlock_unlock(&large_alloc_lock);
            return size;
        }
        curr = curr->next;
    }
    spinlock_unlock(&large_alloc_lock);
    
    // 如果不是大块内存，尝试找到它的slab缓存
    for (int i = 0; i < SLAB_SIZES_COUNT; i++) {
        struct kmem_cache *cache = &slab_caches[i];
        
        spinlock_lock(&cache->lock);
        
        // 检查所有slabs
        int found = 0;
        struct list_head *pos;
        
        list_for_each(pos, &cache->slabs_full) {
            struct slab_header *slab = list_entry(pos, struct slab_header, list);
            void *slab_start = slab + 1;
            void *slab_end = (char *)slab_start + slab->total_count * slab->obj_size;
            
            if (ptr >= slab_start && ptr < slab_end) {
                found = 1;
                break;
            }
        }
        
        if (!found) {
            list_for_each(pos, &cache->slabs_partial) {
                struct slab_header *slab = list_entry(pos, struct slab_header, list);
                void *slab_start = slab + 1;
                void *slab_end = (char *)slab_start + slab->total_count * slab->obj_size;
                
                if (ptr >= slab_start && ptr < slab_end) {
                    found = 1;
                    break;
                }
            }
        }
        
        spinlock_unlock(&cache->lock);
        
        if (found) {
            return cache->obj_size;
        }
    }
    
    // 无法确定大小
    panic("ksize: invalid pointer 0x%lx\n", (uint64)ptr);
    return 0;
}

// 调试函数：打印内存分配器状态
void kmalloc_stats(void) {
    sprint("Kernel memory allocator statistics:\n");
    
    // 打印slab缓存统计
    sprint("Slab caches:\n");
    for (int i = 0; i < SLAB_SIZES_COUNT; i++) {
        struct kmem_cache *cache = &slab_caches[i];
        
        spinlock_lock(&cache->lock);
        
        int full_count = 0, partial_count = 0, free_count = 0;
        struct list_head *pos;
        
        list_for_each(pos, &cache->slabs_full) {
            full_count++;
        }
        
        list_for_each(pos, &cache->slabs_partial) {
            partial_count++;
        }
        
        list_for_each(pos, &cache->slabs_free) {
            free_count++;
        }
        
        sprint("  Size %4d bytes: %2d full, %2d partial, %2d free, %4d free objects\n",
              cache->obj_size, full_count, partial_count, free_count, cache->free_objects);
        
        spinlock_unlock(&cache->lock);
    }
    
    // 打印大块内存分配统计
    spinlock_lock(&large_alloc_lock);
    
    int large_count = 0;
    size_t large_total = 0;
    struct large_allocation *curr = large_allocations;
    
    while (curr != NULL) {
        large_count++;
        large_total += curr->size;
        curr = curr->next;
    }
    
    sprint("Large allocations: %d blocks, %lu bytes total\n", 
          large_count, large_total);
    
    spinlock_unlock(&large_alloc_lock);
    
    // 打印页面统计
    sprint("Free page count: %d\n", get_free_page_count());
}