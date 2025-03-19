#include <errno.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/inode.h>
#include <kernel/mm/kmalloc.h>
#include <util/hashtable.h>
#include <util/string.h>

#include <spike_interface/spike_utils.h>

/* Dentry cache hashtable */
static struct hashtable dentry_hashtable;

static void* dentry_get_key(struct list_head* node);
static unsigned int dentry_hash(const void* key, unsigned int size);
static int dentry_key_equals(const void* k1, const void* k2);

/* 复合键结构 - 用于查找时构建临时键 */
struct dentry_key {
	struct dentry* parent; /* 父目录项 */
	struct qstr* name;     /* 名称 */
};

static struct list_head g_dentry_lru_list;  /* 全局LRU链表，用于dentry的复用 */
static unsigned int g_dentry_lru_count = 0; /* 当前LRU链表中的dentry数量 */
static spinlock_t g_dentry_lru_list_lock;

static struct dentry* __dentry_alloc(struct dentry* parent, const struct qstr* name);
static void __dentry_free(struct dentry* dentry);
static struct dentry* __find_in_lru_list(struct dentry* parent, const struct qstr* name);

void init_dentry_lruList(void) {
	/* 初始化全局LRU链表 */
	INIT_LIST_HEAD(&g_dentry_lru_list);
	spinlock_init(&g_dentry_lru_list_lock);
	g_dentry_lru_count = 0;
}
/**
 * 强化版dentry获取接口 - 集成查找与创建功能
 *
 * @param parent: 父dentry
 * @param name: 需要查找的名称
 * @param is_dir: 文件类型筛选器:
 *               -1: 不指定类型(只匹配名字)
 *                0: 仅匹配文件
 *                1: 仅匹配目录
 * @param revalidate: 是否需要重新验证找到的dentry
 * @param alloc: 未找到时是否创建新dentry
 * @return: 匹配的dentry，引用计数加1，未找到或不符合要求时返回NULL
 */
struct dentry* dentry_get(struct dentry* parent, const struct qstr* name, int is_dir, bool revalidate, bool alloc) {
	struct dentry* dentry = NULL;
	bool type_match = true;

	if (!parent || !name || !name->name)
		return NULL;

	/* 确保名称有哈希值 */
	struct qstr tmp_name = *name;
	if (!tmp_name.hash)
		tmp_name.hash = full_name_hash(tmp_name.name, tmp_name.len);

	/* 1. 先尝试查找已有的dentry */
	dentry = d_lookup(parent, &tmp_name);

	/* 2. 如果找到但需要重新验证 */
	if (dentry && revalidate && dentry->d_operations && dentry->d_operations->d_revalidate) {
		if (!dentry->d_operations->d_revalidate(dentry, 0)) {
			/* 验证失败，放弃此dentry */
			dentry_put(dentry);
			d_drop(dentry);
			dentry = NULL;
		}
	}

	/* 3. 如果找到，检查类型是否匹配 */
	if (dentry && is_dir != -1) {
		/* 获取inode并检查是否为目录 */
		if (dentry->d_inode) {
			bool is_directory = S_ISDIR(dentry->d_inode->i_mode);
			type_match = (is_dir == 1) ? is_directory : !is_directory;
		} else {
			/* 负向dentry，无法确定类型 */
			type_match = false;
		}

		if (!type_match) {
			/* 类型不匹配，放弃此dentry */
			dentry_put(dentry);
			dentry = NULL;
		}
	}

	/* 4. 如果没找到或类型不匹配，尝试从LRU列表复用 */
	if (!dentry) {
		dentry = __find_in_lru_list(parent, &tmp_name);

		/* 如果在LRU中找到可复用的dentry */
		if (dentry) {
			/* 检查类型是否匹配 */
			if (is_dir != -1 && dentry->d_inode) {
				bool is_directory = S_ISDIR(dentry->d_inode->i_mode);
				type_match = (is_dir == 1) ? is_directory : !is_directory;

				if (!type_match) {
					/* 类型不匹配，不使用此dentry */
					__dentry_free(dentry); /* 直接释放而不回到LRU */
					dentry = NULL;
				} else {
					/* 重置dentry状态 */
					atomic_set(&dentry->d_refcount, 1);

					/* 重新添加到哈希表 */
					if (!(dentry->d_flags & DCACHE_HASHED)) {
						int ret = hashtable_insert(&dentry_hashtable, &dentry->d_hashNode);
						if (ret == 0) {
							dentry->d_flags |= DCACHE_HASHED;
						}
					}
				}
			}
		}
	}

	/* 5. 如果依然没有找到且允许创建，则创建新dentry */
	if (!dentry && alloc) {
		/* 创建新dentry */
		dentry = __dentry_alloc(parent, &tmp_name);
		if (dentry) {
			/* 添加到哈希表 */
			int ret = hashtable_insert(&dentry_hashtable, &dentry->d_hashNode);
			if (ret == 0) {
				dentry->d_flags |= DCACHE_HASHED;
			}

			/* 标记为负dentry */
			dentry->d_flags |= DCACHE_NEGATIVE;
		}
	}

	return dentry;
}

/* 保留现有函数，但将创建函数改为内部使用 */
static struct dentry* __dentry_alloc(struct dentry* parent, const struct qstr* name) {
	struct dentry* dentry;

	if (!name || !name->name)
		return NULL;

	/* 分配dentry结构 */
	dentry = kmalloc(sizeof(struct dentry));
	if (!dentry)
		return NULL;

	/* 初始化基本字段 */
	memset(dentry, 0, sizeof(struct dentry));
	spinlock_init(&dentry->d_lock);
	atomic_set(&dentry->d_refcount, 1);
	INIT_LIST_HEAD(&dentry->d_childList);
	INIT_LIST_HEAD(&dentry->d_lruListNode);
	INIT_LIST_HEAD(&dentry->d_aliasListNode);
	INIT_LIST_HEAD(&dentry->d_hashNode);

	/* 复制name */
	dentry->d_name.len = name->len;
	dentry->d_name.hash = name->hash ? name->hash : full_name_hash(name->name, name->len);

	dentry->d_name.name = kmalloc(name->len + 1);
	if (!dentry->d_name.name) {
		kfree(dentry);
		return NULL;
	}

	memcpy((char*)dentry->d_name.name, name->name, name->len);
	((char*)dentry->d_name.name)[name->len] = '\0';

	/* 设置父节点关系 */
	dentry->d_parent = parent ? get_dentry(parent) : dentry; /* 根目录是自己的父节点 */

	if (parent) {
		dentry->d_superblock = parent->d_superblock;

		/* 添加到父节点的子列表 */
		spinlock_lock(&parent->d_lock);
		list_add(&dentry->d_parentListNode, &parent->d_childList);
		spinlock_unlock(&parent->d_lock);
	}

	return dentry;
}

/**
 * 从哈希节点获取dentry键
 */
static void* dentry_get_key(struct list_head* node) {
	static struct dentry_key key;
	struct dentry* dentry = container_of(node, struct dentry, d_hashNode);

	key.parent = dentry->d_parent;
	key.name = &dentry->d_name;

	return &key;
}

/**
 * 计算dentry复合键的哈希值
 */
static unsigned int dentry_hash(const void* key, unsigned int size) {
	const struct dentry_key* dkey = (const struct dentry_key*)key;
	unsigned int hash;

	/* 结合父指针和名称哈希 */
	hash = (unsigned long)dkey->parent;
	hash = hash * 31 + dkey->name->hash;

	return hash;
}

/**
 * 比较两个dentry键是否相等
 */
static int dentry_key_equals(const void* k1, const void* k2) {
	const struct dentry_key* key1 = (const struct dentry_key*)k1;
	const struct dentry_key* key2 = (const struct dentry_key*)k2;

	/* 首先比较父节点 */
	if (key1->parent != key2->parent)
		return 0;

	/* 然后比较名称 */
	const struct qstr* name1 = key1->name;
	const struct qstr* name2 = key2->name;

	if (name1->len != name2->len)
		return 0;

	return !memcmp(name1->name, name2->name, name1->len);
}

/**
 * 初始化dentry缓存
 */
int init_dentry_hashtable(void) {
	sprint("Initializing dentry hashtable\n");

	/* 初始化dentry哈希表 */
	return hashtable_setup(&dentry_hashtable, 1024, /* 初始桶数 */
	                       75,                      /* 负载因子 */
	                       dentry_hash, dentry_get_key, dentry_key_equals);
}

/**
 * 在父dentry中查找子dentry
 */
struct dentry* d_lookup(const struct dentry* parent, const struct qstr* name) {
	struct dentry_key key;
	struct list_head* node;
	struct dentry* dentry = NULL;

	if (!parent || !name)
		return NULL;

	/* 构建查询键 */
	key.parent = (struct dentry*)parent;
	key.name = (struct qstr*)name;

	/* 在哈希表中查找 */
	node = hashtable_lookup(&dentry_hashtable, &key);

	if (node) {
		dentry = container_of(node, struct dentry, d_hashNode);
		get_dentry(dentry); /* 增加引用计数 */
	}

	return dentry;
}

/**
 * 计算名称哈希并查找
 */
struct dentry* d_hash_and_lookup(struct dentry* parent, struct qstr* name) {
	/* 如果名称还没有计算哈希值，先计算 */
	if (!name->hash)
		name->hash = full_name_hash(name->name, name->len);

	return d_lookup(parent, name);
}

/**
 * 释放dentry引用
 */
int dentry_put(struct dentry* dentry) {
	if (!dentry)
		return -EINVAL;
	if (atomic_read(&dentry->d_refcount) <= 0)
		return -EINVAL;
	/* 如果引用计数降为0 */
	if (atomic_dec_and_test(&dentry->d_refcount)) {
		/* 如果是未使用但已哈希的dentry，加入LRU列表而非释放 */
		/* 加入LRU列表的代码，需要全局LRU列表的锁 */
		spinlock_lock(&g_dentry_lru_list_lock);
		list_add(&dentry->d_lruListNode, &g_dentry_lru_list);
		g_dentry_lru_count++;
		spinlock_unlock(&g_dentry_lru_list_lock);
		/* 注意：这里不需要释放dentry，因为它会在内存压力时被回收 */
	}
	return 0;
}

/**
 * 从缓存中删除dentry
 */
static void __dentry_free(struct dentry* dentry) {
	if (!dentry)
		return;

	spinlock_lock(&dentry->d_lock);

	/* 从哈希表中移除 */
	if (dentry->d_flags & DCACHE_HASHED) {
		hashtable_remove(&dentry_hashtable, &dentry->d_hashNode);
		dentry->d_flags &= ~DCACHE_HASHED;
	}

	/* 从父节点的子列表中移除 */
	if (!list_empty(&dentry->d_parentListNode)) {
		list_del(&dentry->d_parentListNode);
		INIT_LIST_HEAD(&dentry->d_parentListNode);
	}

	/* 从LRU列表中移除 */
	if (!list_empty(&dentry->d_lruListNode)) {
		list_del(&dentry->d_lruListNode);
		INIT_LIST_HEAD(&dentry->d_lruListNode);
	}

	/* 从inode的别名列表中移除 */
	if (dentry->d_inode && !list_empty(&dentry->d_aliasListNode)) {
		spinlock_lock(&dentry->d_inode->i_dentryList_lock);
		list_del_init(&dentry->d_aliasListNode);
		spinlock_unlock(&dentry->d_inode->i_dentryList_lock);
	}

	spinlock_unlock(&dentry->d_lock);

	/* 释放inode引用 */
	if (dentry->d_inode) {
		inode_put(dentry->d_inode);
		dentry->d_inode = NULL;
	}

	/* 释放父引用 */
	if (dentry->d_parent && dentry->d_parent != dentry) {
		dentry_put(dentry->d_parent);
		dentry->d_parent = NULL;
	}

	/* 释放名称 */
	if (dentry->d_name.name) {
		kfree((void*)dentry->d_name.name);
		dentry->d_name.name = NULL;
	}

	/* 释放dentry结构 */
	kfree(dentry);
}

/**
 * 从哈希表中暂时移除dentry，但不释放
 */
void d_drop(struct dentry* dentry) {
	if (!dentry)
		return;

	spinlock_lock(&dentry->d_lock);

	/* 从哈希表中移除 */
	if (dentry->d_flags & DCACHE_HASHED) {
		hashtable_remove(&dentry_hashtable, &dentry->d_hashNode);
		dentry->d_flags &= ~DCACHE_HASHED;
	}

	spinlock_unlock(&dentry->d_lock);
}

/**
 * 将dentry重新添加到哈希表
 */
void d_rehash(struct dentry* dentry) {
	if (!dentry)
		return;

	spinlock_lock(&dentry->d_lock);

	if (!(dentry->d_flags & DCACHE_HASHED)) {
		int ret = hashtable_insert(&dentry_hashtable, &dentry->d_hashNode);
		if (ret == 0) {
			dentry->d_flags |= DCACHE_HASHED;
		}
	}

	spinlock_unlock(&dentry->d_lock);
}

/**
 * 在父dentry中查找子dentry，若不存在则创建
 */
struct dentry* d_lookup_or_create(struct dentry* parent, const struct qstr* name) {
	struct dentry* dentry;

	/* 先查找现有dentry */
	dentry = d_lookup(parent, name);

	/* 如果不存在，创建新的 */
	if (!dentry) {
		dentry = d_alloc_qstr(parent, name);
		if (dentry) {
			/* 添加到哈希表 */
			hashtable_insert(&dentry_hashtable, &dentry->d_hashNode);
			dentry->d_flags |= DCACHE_HASHED;
		}
	}

	return dentry;
}

/**
 * 从LRU列表中释放指定数量的dentry
 * 通常在内存压力大时调用
 *
 * @param count: 要释放的dentry数量，0表示全部释放
 * @return: 实际释放的数量
 */
unsigned int shrink_dentry_lru(unsigned int count) {
	struct dentry* dentry;
	struct list_head *pos, *n;
	unsigned int freed = 0;

	spinlock_lock(&g_dentry_lru_list_lock);

	/* 如果count为0，释放全部 */
	if (count == 0)
		count = g_dentry_lru_count;

	list_for_each_safe(pos, n, &g_dentry_lru_list) {
		if (freed >= count)
			break;

		dentry = container_of(pos, struct dentry, d_lruListNode);

		/* 从LRU列表移除 */
		list_del_init(&dentry->d_lruListNode);
		g_dentry_lru_count--;

		/* 释放dentry */
		__dentry_free(dentry);

		freed++;
	}

	spinlock_unlock(&g_dentry_lru_list_lock);
	return freed;
}

/**
 * 在LRU列表中查找可复用的dentry
 * 如果找到匹配的dentry会将其从LRU列表移除
 */
static struct dentry* __find_in_lru_list(struct dentry* parent, const struct qstr* name) {
	struct dentry* dentry;
	struct dentry* result = NULL;
	struct list_head *pos, *n;

	if (!parent || !name)
		return NULL;

	spinlock_lock(&g_dentry_lru_list_lock);

	list_for_each_safe(pos, n, &g_dentry_lru_list) {
		dentry = container_of(pos, struct dentry, d_lruListNode);

		/* 检查父节点和名称是否匹配 */
		if (dentry->d_parent == parent && dentry->d_name.len == name->len && !memcmp(dentry->d_name.name, name->name, name->len)) {

			/* 从LRU列表中移除 */
			list_del_init(&dentry->d_lruListNode);
			g_dentry_lru_count--;

			result = dentry;
			break;
		}
	}

	spinlock_unlock(&g_dentry_lru_list_lock);
	return result;
}

/**
 * 将dentry与inode关联
 *
 * @param dentry: 要关联的dentry
 * @param inode: 要关联的inode，NULL表示创建负向dentry（未实现）
 * @return: 成功返回0，失败返回错误码
 */
int dentry_set_inode(struct dentry* dentry, struct inode* inode) {
	if (!dentry || !inode)
		return -EINVAL;

	spinlock_lock(&dentry->d_lock);

	/* 如果已有inode，先解除关联 */
	if (dentry->d_inode) {
		/* 从inode的别名列表中移除 */
		if (!list_empty(&dentry->d_aliasListNode)) {
			spinlock_lock(&dentry->d_inode->i_dentryList_lock);
			list_del_init(&dentry->d_aliasListNode);
			spinlock_unlock(&dentry->d_inode->i_dentryList_lock);
		}

		/* 减少inode引用 */
		inode_put(dentry->d_inode);
		dentry->d_inode = NULL;
	}

	/* 增加inode引用计数 */
	inode = iget(inode);
	dentry->d_inode = inode;

	/* 添加到inode的别名列表 */
	spinlock_lock(&inode->i_dentryList_lock);
	list_add(&dentry->d_aliasListNode, &inode->i_dentryList);
	spinlock_unlock(&inode->i_dentryList_lock);

	/* 清除负向dentry标志 */
	dentry->d_flags &= ~DCACHE_NEGATIVE;

	spinlock_unlock(&dentry->d_lock);
	return 0;
}