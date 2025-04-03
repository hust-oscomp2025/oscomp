#include <kernel/device/buffer_head.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/types.h>
#include <kernel/util/hashtable.h>
#include <kernel/util/list.h>
#include <kernel/util/string.h>

#include <kernel/util/print.h>

// 缓冲区哈希表
static struct hashtable buffer_hash;

// LRU 列表和锁
struct list_head bh_lru_list;
spinlock_t bh_lru_lock;

// 缓冲区的键结构
struct buffer_key {
	struct block_device* bdev;
	sector_t block;
	size_t size;
};

// 哈希函数 - 为 buffer_key 生成哈希值
static uint32 buffer_hash_func(const void* key) {
	const struct buffer_key* bkey = (const struct buffer_key*)key;
	// 组合设备指针、块号和大小以生成哈希值
	uint32 hash = (uint32)(uintptr_t)bkey->bdev;
	hash = hash * 37 + (uint32)bkey->block;
	hash = hash * 37 + (uint32)bkey->size;
	return hash;
}

// 从缓冲区节点获取键
static void* buffer_get_key(struct list_head* node) {
	struct buffer_head* bh = container_of(node, struct buffer_head, b_lru);
	static struct buffer_key key;

	key.bdev = bh->b_bdev;
	key.block = bh->b_blocknr;
	key.size = bh->b_size;

	return &key;
}

// 比较两个缓冲区键
static int32 buffer_key_equals(const void* key1, const void* key2) {
	const struct buffer_key* k1 = (const struct buffer_key*)key1;
	const struct buffer_key* k2 = (const struct buffer_key*)key2;

	return k1->bdev == k2->bdev && k1->block == k2->block && k1->size == k2->size;
}

// 分配一个新的 buffer_head
// struct buffer_head *alloc_buffer_head(gfp_t gfp_flags) {
struct buffer_head* alloc_buffer_head(void) {
	struct buffer_head* bh;
	// bh = kmalloc(sizeof(struct buffer_head), gfp_flags);
	bh = kmalloc(sizeof(struct buffer_head));

	if (bh) {
		memset(bh, 0, sizeof(struct buffer_head));
		INIT_LIST_HEAD(&bh->b_lru);
		spinlock_init(&bh->b_lock);
		atomic_set(&bh->b_count, 0);
	}
	return bh;
}

// 释放一个 buffer_head
void free_buffer_head(struct buffer_head* bh) {
	if (bh) {
		if (bh->b_data) kfree(bh->b_data);
		kfree(bh);
	}
}

// 获取一个缓冲区，不读取数据
struct buffer_head* getblk(struct block_device* bdev, sector_t block, size_t size) {
	struct buffer_head* bh;
	struct buffer_key key;
	struct list_head* node;

	// 设置查找键
	key.bdev = bdev;
	key.block = block;
	key.size = size;

	// 在哈希表中查找
	node = hashtable_lookup(&buffer_hash, &key);
	if (node) {
		// 找到了，增加引用计数
		bh = container_of(node, struct buffer_head, b_lru);
		atomic_inc(&bh->b_count);

		// 更新 LRU 位置 (从当前位置移除并添加到尾部)
		spinlock_lock(&bh_lru_lock);
		if (!list_empty(&bh->b_lru)) {
			list_del(&bh->b_lru);
			list_add_tail(&bh->b_lru, &bh_lru_list);
		}
		spinlock_unlock(&bh_lru_lock);

		return bh;
	}

	// 未找到，分配一个新的
	// bh = alloc_buffer_head(GFP_KERNEL);
	bh = alloc_buffer_head();

	if (!bh) return NULL;

	bh->b_bdev = bdev;
	bh->b_blocknr = block;
	bh->b_size = size;
	// bh->b_data = kmalloc(size, GFP_KERNEL);
	bh->b_data = kmalloc(size);

	if (!bh->b_data) {
		free_buffer_head(bh);
		return NULL;
	}

	atomic_set(&bh->b_count, 1);
	set_buffer_new(bh);
	set_buffer_mapped(bh);

	// 添加到哈希表
	if (hashtable_insert(&buffer_hash, &bh->b_lru) != 0) {
		// 插入失败，释放资源
		kfree(bh->b_data);
		free_buffer_head(bh);
		return NULL;
	}

	// 添加到LRU列表
	spinlock_lock(&bh_lru_lock);
	list_add_tail(&bh->b_lru, &bh_lru_list);
	spinlock_unlock(&bh_lru_lock);

	return bh;
}

// 获取一个缓冲区并读取数据
struct buffer_head* bread(struct block_device* bdev, sector_t block, size_t size) {
	struct buffer_head* bh = getblk(bdev, block, size);
	if (!bh) return NULL;

	if (!buffer_uptodate(bh)) {
		lock_buffer(bh);
		if (!buffer_uptodate(bh)) { // 双重检查，避免竞争条件
			if (bdev->bd_ops->read_blocks(bdev, bh->b_data, block, 1) != 0) {
				unlock_buffer(bh);
				brelse(bh);
				return NULL;
			}
			set_buffer_uptodate(bh);
		}
		unlock_buffer(bh);
	}

	return bh;
}

// 释放对缓冲区的引用
void brelse(struct buffer_head* bh) {
	if (!bh) return;

	if (atomic_dec_and_test(&bh->b_count)) {
		// 最后一个引用被释放
		// 如果是脏缓冲区，需要写回
		if (buffer_dirty(bh)) { sync_dirty_buffer(bh); }

		// 从哈希表中移除（可选，取决于缓存策略）
		// hashtable_remove(&buffer_hash, &bh->b_lru);

		// 保持在缓存中，等待LRU淘汰
	}
}

// 标记缓冲区为脏
void mark_buffer_dirty(struct buffer_head* bh) {
	if (bh) set_buffer_dirty(bh);
}

// 提交一个脏缓冲区
int32 sync_dirty_buffer(struct buffer_head* bh) {
	int32 ret = 0;

	if (!buffer_dirty(bh)) return 0;

	lock_buffer(bh);
	if (buffer_dirty(bh)) {
		ret = bh->b_bdev->bd_ops->write_blocks(bh->b_bdev, bh->b_data, bh->b_blocknr, 1);
		if (ret == 0) clear_buffer_dirty(bh);
	}
	unlock_buffer(bh);

	return ret;
}

// 异步读写块（简化实现，实际应该使用异步 I/O）
void ll_rw_block(int32 rw, int32 nr, struct buffer_head* bhs[]) {
	int32 i;

	for (i = 0; i < nr; i++) {
		struct buffer_head* bh = bhs[i];

		if (!bh) continue;

		if (rw == READ) {
			if (!buffer_uptodate(bh)) {
				lock_buffer(bh);
				bh->b_bdev->bd_ops->read_blocks(bh->b_bdev,bh->b_data, bh->b_blocknr,  1);
				set_buffer_uptodate(bh);
				unlock_buffer(bh);
			}
		} else {
			if (buffer_dirty(bh)) {
				lock_buffer(bh);
				bh->b_bdev->bd_ops->write_blocks(bh->b_bdev,bh->b_data, bh->b_blocknr,  1);
				clear_buffer_dirty(bh);
				unlock_buffer(bh);
			}
		}
	}
}

// 锁住一个缓冲区
void lock_buffer(struct buffer_head* bh) {
	spinlock_lock(&bh->b_lock);
	set_buffer_locked(bh);
}

// 解锁一个缓冲区
void unlock_buffer(struct buffer_head* bh) {
	clear_buffer_locked(bh);
	spinlock_unlock(&bh->b_lock);
}

// 等待缓冲区操作完成
void wait_on_buffer(struct buffer_head* bh) {
	while (buffer_locked(bh)) {
		// 在实际实现中，这里应该使用睡眠/唤醒机制
		// 简单起见，使用简单的轮询
		// yield_cpu(); // 让出CPU时间
	}
}

// 初始化buffer_head子系统
void buffer_init(void) {
	// 初始化哈希表，大小为1024，最大负载因子为80%
	int32 ret = hashtable_setup(&buffer_hash, 1024, 80, buffer_hash_func, buffer_get_key, buffer_key_equals);
	if (ret != 0) {
		// 初始化失败处理
		panic("Failed to initialize buffer cache hash table");
	}

	// 初始化LRU列表
	INIT_LIST_HEAD(&bh_lru_list);
	spinlock_init(&bh_lru_lock);
}

// 同步脏缓冲区
int32 sync_dirty_buffers(struct block_device* bdev) {
	struct buffer_head* bh;
	int32 ret = 0;

	// 这里应该遍历所有与该设备相关的缓冲区并同步它们
	// 但由于哈希表的设计，我们无法直接获取特定设备的所有缓冲区
	// 需要实现一个额外的设备->缓冲区映射或修改哈希表接口

	// 简单起见，遍历LRU列表查找该设备的脏缓冲区
	spinlock_lock(&bh_lru_lock);
	list_for_each_entry(bh, &bh_lru_list, b_lru) {
		if (bh->b_bdev == bdev && buffer_dirty(bh)) {
			int32 err = sync_dirty_buffer(bh);
			if (err && !ret) ret = err;
		}
	}
	spinlock_unlock(&bh_lru_lock);

	return ret;
}