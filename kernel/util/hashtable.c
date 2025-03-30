#include <errno.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/util/hashtable.h>
#include <kernel/util/string.h>

/**
 * 初始化预先静态分配的哈希表
 */
int32 hashtable_setup(struct hashtable* ht, uint32 initial_size, uint32 max_load, uint32 (*hash_func)(const void* key), void* (*get_key)(struct list_head* node),
                   int32 (*key_equals)(const void* key1, const void* key2)) {
	uint32 i;

	if (!ht || !hash_func || !get_key || !key_equals)
		return -EINVAL;

	/* 默认大小和负载因子 */
	if (initial_size < 16)
		initial_size = 16;
	if (max_load < 50 || max_load > 90)
		max_load = 70;

	/* 调整为2的幂 */
	initial_size = next_power_of_2(initial_size);

	/* 分配桶数组 */
	ht->buckets = kmalloc(initial_size * sizeof(struct hash_bucket));
	if (!ht->buckets)
		return -ENOMEM;

	/* 初始化每个桶的链表头 */
	for (i = 0; i < initial_size; i++) {
		INIT_LIST_HEAD(&ht->buckets[i].head);
		spinlock_init(&ht->buckets[i].lock);
	}

	/* 初始化表参数 */
	ht->size = initial_size;
	atomic_set(&ht->items, 0);
	ht->max_load = max_load;
	ht->hash_func = hash_func;
	ht->get_key = get_key;
	ht->key_equals = key_equals;

	return 0;
}

/**
 * 增量扩容哈希表 - 使用位掩码技术避免重新哈希
 * 每次扩容时只处理少量桶，减少阻塞
 */
static int32 hashtable_try_expand(struct hashtable* ht) {
	static struct hash_bucket* new_buckets = NULL;
	static uint32 new_size = 0;
	static uint32 transfer_index = 0;
	static int32 expansion_in_progress = 0;

	uint32 current_items = atomic_read(&ht->items);
	uint32 threshold = ht->size * ht->max_load / 100;

	/* 检查是否需要扩容 */
	if (current_items < threshold)
		return 0;

	/* 尝试开始扩容过程 */
	if (!expansion_in_progress) {
		/* 只有一个线程能进入扩容流程 */
		if (__sync_val_compare_and_swap(&ht->expanding, 0, 1) != 0)
			return 0;

		/* 我们获得了扩容权限 */
		new_size = ht->size * 2;
		new_buckets = kmalloc(new_size * sizeof(struct hash_bucket));
		if (!new_buckets) {
			ht->expanding = 0;
			return -ENOMEM;
		}

		/* 初始化新桶 */
		for (uint32 i = 0; i < new_size; i++) {
			INIT_LIST_HEAD(&new_buckets[i].head);
			spinlock_init(&new_buckets[i].lock);
		}

		transfer_index = 0;
		expansion_in_progress = 1;
	}

	/* 一次最多传输16个桶，避免长时间持有锁 */
	uint32 end_index = transfer_index + 16;
	if (end_index > ht->size)
		end_index = ht->size;

	/* 复制这一批桶的数据 - 使用位掩码技术 */
	for (uint32 i = transfer_index; i < end_index; i++) {
		struct hash_bucket* old_bucket = &ht->buckets[i];
		struct list_head *node, *tmp;

		/* 锁定当前桶 */
		spinlock_lock(&old_bucket->lock);

		list_for_each_safe(node, tmp, &old_bucket->head) {
			void* key = ht->get_key(node);

			/*
			 * 关键优化: 无需重新计算哈希值
			 * 只需判断原哈希的高位决定节点去向
			 *
			 * 注意: 我们这里假设hash_func返回的是完整哈希值，不依赖表大小
			 * 桶索引是通过 hash & (size-1) 计算的
			 */
			uint32 hash = ht->hash_func(key); // 获取完整哈希值
			uint32 new_idx;

			/*
			 * 检查哈希值的第log2(ht->size)位:
			 * 如果为0: 节点保持在与原桶相同的索引
			 * 如果为1: 节点移动到 原索引+old_size 的位置
			 */
			if (hash & ht->size) {
				new_idx = i + ht->size; // 高位为1，放入新桶
			} else {
				new_idx = i; // 高位为0，保持原桶索引
			}

			/* 从旧链表移除 */
			list_del(node);

			/* 锁定目标桶并添加节点 */
			spinlock_lock(&new_buckets[new_idx].lock);
			list_add(node, &new_buckets[new_idx].head);
			spinlock_unlock(&new_buckets[new_idx].lock);
		}

		spinlock_unlock(&old_bucket->lock);
	}

	transfer_index = end_index;

	/* 检查是否完成所有桶转移 */
	if (transfer_index >= ht->size) {
		/* 所有桶都已转移，替换桶数组 */
		struct hash_bucket* old_buckets = ht->buckets;
		ht->buckets = new_buckets;
		ht->size = new_size;
		kfree(old_buckets);

		/* 重置扩容状态 */
		new_buckets = NULL;
		new_size = 0;
		transfer_index = 0;
		expansion_in_progress = 0;
		ht->expanding = 0;
	}

	return 0;
}
/**
 * 向哈希表插入节点
 */
int32 hashtable_insert(struct hashtable* ht, struct list_head* node) {
	void* key;
	uint32 hash, idx;
	struct list_head* pos;
	int32 ret = 0;

	if (!ht || !node)
		return -EINVAL;

	key = ht->get_key(node);
	if (!key)
		return -EINVAL;

	/* 计算哈希值和索引 */
	hash = ht->hash_func(key);
	idx = hash & (ht->size - 1);

	/* 检查是否需要扩容 */
	if (hashtable_count(ht) >= ht->size * ht->max_load / 100) {
		ret = hashtable_try_expand(ht);
		if (ret < 0) {
			return ret;
		}
		/* 重新计算索引，因为大小已经改变 */
		idx = hash & (ht->size - 1);
	}
	spinlock_lock(&ht->buckets[idx].lock);
	/* 检查键是否已存在 */
	list_for_each(pos, &ht->buckets[idx].head) {
		void* existing_key = ht->get_key(pos);
		if (ht->key_equals(existing_key, key)) {
			/* 键已存在，这里我们不替换，只返回错误 */
			spinlock_unlock(&ht->buckets[idx].lock);
			return -EEXIST;
		}
	}

	/* 添加到链表 */
	list_add(node, &ht->buckets[idx].head);
	atomic_inc(&ht->items);

	spinlock_unlock(&ht->buckets[idx].lock);
	return 0;
}

/**
 * 在哈希表中查找键
 */
struct list_head* hashtable_lookup(struct hashtable* ht, const void* key) {
	uint32 hash, idx;
	struct list_head* pos;
	struct list_head* result = NULL;

	if (!ht || !key)
		return NULL;

	/* 计算哈希值和索引 */
	hash = ht->hash_func(key);
	idx = hash & (ht->size - 1);

	spinlock_lock(&ht->buckets[idx].lock);

	/* 在链表中查找 */
	list_for_each(pos, &ht->buckets[idx].head) {
		void* node_key = ht->get_key(pos);
		if (ht->key_equals(node_key, key)) {
			result = pos;
			break;
		}
	}

	spinlock_unlock(&ht->buckets[idx].lock);
	return result;
}

/**
 * 从哈希表中删除节点
 */
int32 hashtable_remove(struct hashtable* ht, struct list_head* node) {
	if (!ht || !node)
		return -EINVAL;
	void* key = ht->get_key(node);
	if (!key)
		return -EINVAL;

	/* 计算哈希值和索引 */
	uint32 hash = ht->hash_func(key);
	uint32 idx = hash & (ht->size - 1);
	spinlock_lock(&ht->buckets[idx].lock);

	/* 检查节点是否在链表中(通过检查是否被正确链接) */
	if (!list_empty(node)) {
		list_del_init(node);
		atomic_dec(&ht->items);
		spinlock_unlock(&ht->buckets[idx].lock);
		return 0;
	}

	spinlock_unlock(&ht->buckets[idx].lock);
	return -ENOENT; /* 节点不在哈希表中 */
}

/**
 * 从哈希表中按键删除节点
 */
int32 hashtable_remove_by_key(struct hashtable* ht, const void* key) {
	uint32 hash, idx;
	struct list_head *pos, *tmp;
	int32 found = 0;

	if (!ht || !key)
		return -EINVAL;

	/* 计算哈希值和索引 */
	hash = ht->hash_func(key);
	idx = hash & (ht->size - 1);

	spinlock_lock(&ht->buckets[idx].lock);

	/* 在链表中查找并删除 */
	list_for_each_safe(pos, tmp, &ht->buckets[idx].head) {
		void* node_key = ht->get_key(pos);
		if (ht->key_equals(node_key, key)) {
			list_del_init(pos);
			atomic_dec(&ht->items);
			spinlock_unlock(&ht->buckets[idx].lock);
			return 0;
		}
	}

	spinlock_unlock(&ht->buckets[idx].lock);

	return -ENOENT;
}

/**
 * 销毁哈希表
 */
void hashtable_clear(struct hashtable* ht) {
	if (!ht || !ht->buckets)
		return;

	/* 释放桶数组 */
	kfree(ht->buckets);
	ht->buckets = NULL;
	ht->size = 0;
	atomic_set(&ht->items, 0);
}

/**
 * 计算字符串的哈希值(FNV-1a算法)
 */
uint32 hash_string(const void* key, uint32 size) {
	const unsigned char* str = (const unsigned char*)key;
	uint32 hash = 2166136261u; /* FNV offset basis */

	while (*str) {
		hash ^= *str++;
		hash *= 16777619; /* FNV prime */
	}

	return hash;
}

/**
 * 计算整数的哈希值(混合整数哈希)
 */
uint32 hash_int(const void* key, uint32 size) {
	uint32 k = *(const uint32*)key;
	k ^= k >> 16;
	k *= 0x85ebca6b;
	k ^= k >> 13;
	k *= 0xc2b2ae35;
	k ^= k >> 16;
	return k;
}

/**
 * 计算指针的哈希值
 */
uint32 hash_ptr(const void* key, uint32 size) {
	uintptr_t ptr = (uintptr_t)key;
	return hash_int(&ptr, size);
}