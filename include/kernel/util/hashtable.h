#ifndef _HASHTABLE_H
#define _HASHTABLE_H

#include <kernel/types.h>
#include <kernel/util/list.h>
#include <kernel/util/spinlock.h>

/**
 * 哈希表结构 - 使用container_of模式
 * 设计用于高效地存储对象的哈希表，对象应内嵌list_head节点
 */
struct hashtable {
	/* 配置参数 */
	uint32 size;     /* 桶数量 */
	atomic_t items;    /* 当前元素数量 */
	uint32 max_load; /* 最大负载百分比(0-100) */
	int32 expanding; /* 是否正在扩容 */

	struct hash_bucket {
		struct list_head head; /* 链表头 */
		spinlock_t lock;       /* 桶级锁 */
	}* buckets;

	/* 回调函数 */
	uint32 (*hash_func)(const void* key); /* 返回完整哈希值，用来支持2幂的哈希表大小 */
	void* (*get_key)(struct list_head* node);                      /* 从节点获取键 */
	int32 (*key_equals)(const void* key1, const void* key2);         /* 比较两个键 */
};

/**
 * 初始化哈希表
 * @ht: 待初始化的哈希表
 * @initial_size: 初始桶数量(会调整为2的幂)
 * @max_load: 最大负载因子百分比(通常70-80)
 * @hash_func: 哈希函数
 * @get_key: 从节点获取键的回调函数
 * @key_equals: 键比较函数
 *
 * 返回: 成功返回0，失败返回负错误码
 */
int32 hashtable_setup(struct hashtable* ht, uint32 initial_size, uint32 max_load, uint32 (*hash_func)(const void* key), void* (*get_key)(struct list_head* node),
                   int32 (*key_equals)(const void* key1, const void* key2));

/**
 * 向哈希表插入节点
 * @ht: 哈希表
 * @node: 要插入的节点(list_head结构，通常嵌入在对象中)
 *
 * 返回: 成功返回0，失败返回负错误码
 * 注意: 如果键已存在，此函数不会更新现有节点
 */
int32 hashtable_insert(struct hashtable* ht, struct list_head* node);

/**
 * 在哈希表中查找键
 * @ht: 哈希表
 * @key: 要查找的键
 *
 * 返回: 找到则返回关联节点，未找到返回NULL
 */
struct list_head* hashtable_lookup(struct hashtable* ht, const void* key);

/**
 * 从哈希表中删除节点
 * @ht: 哈希表
 * @node: 要删除的节点
 *
 * 返回: 成功返回0，节点不在表中返回-ENOENT
 */
int32 hashtable_remove(struct hashtable* ht, struct list_head* node);

/**
 * 从哈希表中按键删除节点
 * @ht: 哈希表
 * @key: 要删除的键
 *
 * 返回: 成功返回0，键不存在返回-ENOENT
 */
int32 hashtable_remove_by_key(struct hashtable* ht, const void* key);

/**
 * 销毁哈希表并释放资源
 * @ht: 要销毁的哈希表
 *
 * 注意: 此函数不会释放或清理节点内容，仅释放哈希表本身的资源
 */
void hashtable_clear(struct hashtable* ht);

/**
 * 获取哈希表中项数
 * @ht: 哈希表
 *
 * 返回: 存储的项数
 */
static inline uint32 hashtable_count(struct hashtable* ht) { return ht ? atomic_read(&ht->items) : 0; }

/**
 * 计算字符串的哈希值
 */
uint32 hash_string(const void* key, uint32 size);

/**
 * 计算整数的哈希值
 */
uint32 hash_int(const void* key, uint32 size);

/**
 * 计算指针的哈希值
 */
uint32 hash_ptr(const void* key, uint32 size);

/**
 * 计算下一个2的幂
 */
static inline uint32 next_power_of_2(uint32 x) {
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x + 1;
}

#endif /* _HASHTABLE_H */