#pragma once

#include "forward_declarations.h"
#include <kernel/util.h>
//#include <kernel/vfs.h>
#include <kernel/fs/vfs/inode.h>

struct dentry_operations;
struct iattr;
/*
 * Directory entry (dentry) structure
 *
 * A dentry is the glue that connects inodes and paths. Each path component
 * (directory or file name) is represented by a dentry.
 */
struct dentry {
	spinlock_t d_lock;   /* Protects dentry fields */
	atomic_t d_refcount; /* Reference count */

	/* RCU lookup touched fields */
	uint32 d_flags;        /* Dentry flags */
	struct inode* d_inode; /* Associated inode */

	/* Lookup cache information */
	struct qstr* d_name;         /* Name of this dentry */
	struct list_node d_hashNode; /* Lookup hash list */

	struct dentry* d_parent;           /* Parent dentry */
	struct list_node d_parentListNode; /* Child entry in parent's list */

	/* Linked list of child dentries */
	struct list_head d_childList; /* Child dentries */

	/* Dentry usage management */

	/* Filesystem and operations */
	struct superblock* d_superblock; /* Superblock of file */

	/* D-cache management */
	uint64 d_time;  /* Revalidation time */
	void* d_fsdata; /* Filesystem-specific data */

	struct list_node d_lruListNode;   /* 全局dentry的LRU链表，在引用计数归零时加入，便于复用 */
	                                  /* 只在内存压力时释放*/
	struct list_node d_inodeListNode; /* Inode 别名链表，用来维护硬链接 */

	/* Mount management */
	// int32 d_mounted;          /* Mount count */
	// struct path* d_automount; /* Automount point */

	const struct dentry_operations* d_op; /* Dentry operations */
};


int32 init_dentry_hashtable(void);
struct dentry* dentry_mkdir(struct dentry* parent, const char* name, fmode_t mode);
struct dentry* dentry_acquire(struct dentry* parent, const struct qstr* name, int32 is_dir, bool revalidate, bool alloc);
struct dentry* dentry_acquireRaw(struct dentry* parent, const char* name, int32 is_dir, bool revalidate, bool alloc);
struct dentry* dentry_ref(struct dentry* dentry);
int32 dentry_unref(struct dentry* dentry);
struct dentry* dentry_mknod(struct dentry* parent, const char* name, mode_t mode, dev_t dev);
struct vfsmount* dentry_lookupMountpoint(struct dentry* dentry);


/*上面是仔细调过的，下面是没调的*/


/*dentry lru链表相关*/
void init_dentry_lruList(void);
uint32 shrink_dentry_lru(uint32 count);

/*dentry生命周期*/


struct dentry* dentry_lookup(struct dentry* parent, const struct qstr* name);

void dentry_prune(struct dentry* dentry); /*unsafe*/
int32 dentry_delete(struct dentry* dentry);

int32 dentry_instantiate(struct dentry* dentry, struct inode* inode);

/*名字和目录操作*/
int32 dentry_rename(struct dentry* old_dentry, struct dentry* new_dentry);

/*符号链接支持*/
// struct dentry* dentry_follow_link(struct dentry* link_dentry);
char* dentry_allocFullPath(struct dentry* dentry);
void dentry_prune(struct dentry* dentry); /*从父目录的子列表和哈希表中分离，但保留结构体和资源*/

/*用于网络文件系统等需要验证缓存有效性的场景*/
int32 dentry_revalidate(struct dentry* dentry, uint32 flags);

int32 setattr_prepare(struct dentry* dentry, struct iattr* attr);
int32 notify_change(struct dentry* dentry, struct iattr* attr);

/*inode hook functions*/
int32 dentry_permission(struct dentry* dentry, int32 mask);
//int32 dentry_getxattr(struct dentry* dentry, const char* name, void* value, size_t size);
//int32 dentry_setxattr(struct dentry* dentry, const char* name, const void* value, size_t size, int32 flags);
//int32 dentry_removexattr(struct dentry* dentry, const char* name);

/*
 * Operations that can be specialized for a particular dentry
 */
struct dentry_operations {
	/* Called to determine if dentry is still valid - important for NFS etc */
	int32 (*d_revalidate)(struct dentry*, uint32);

	/* Called to hash the dentry name for dcache */
	int32 (*d_hash)(const struct dentry*, struct qstr*);

	/* Called for name comparisons */
	int32 (*d_compare)(const struct dentry*, uint32, const char*, const struct qstr*);

	/* Called when dentry's reference count reaches 0 */
	int32 (*d_free)(const struct dentry*);

	/* Called to release dentry's inode */
	void (*d_inode_put)(struct dentry*, struct inode*);

	/* Called to create the relative path of a dentry */
	char* (*d_dname)(struct dentry*, char*, int32);

	/* Called when a dentry is unhashed */
	void (*d_prune)(struct dentry*);
};

/*
 * Dentry state flags
 */
#define DCACHE_DISCONNECTED 0x0001   /* Dentry is disconnected from the FS tree */
#define DCACHE_OP_HASH 0x0002        /* Has a custom hash operation */
#define DCACHE_OP_COMPARE 0x0004     /* Has a custom compare operation */
#define DCACHE_OP_REVALIDATE 0x0008  /* Has a revalidate operation */
#define DCACHE_OP_DELETE 0x0010      /* Has a delete operation */
#define DCACHE_REFERENCED 0x0040     /* Recently used */
#define DCACHE_RCUACCESS 0x0080      /* RCU-enabled */
#define DCACHE_NEGATIVE 0x0100       /* Negative dentry (no inode exists) */
#define DCACHE_FALLTHRU 0x0200       /* Used for lookup fallthrough */
#define DCACHE_NEED_AUTOMOUNT 0x1000 /* Automount point */
#define DCACHE_MOUNTED 0x2000        /* Is a mountpoint */
#define DCACHE_HASHED 0x4000         /* In dentry hash table */
#define DCACHE_IN_LRU 0x8000         /* Dentry在LRU列表中 */

static inline bool dentry_isDir(const struct dentry* dentry) {
	if (!dentry || !dentry->d_inode) return false;
	return S_ISDIR(dentry->d_inode->i_mode);
}

static inline bool dentry_isSymlink(const struct dentry* dentry) {
	if (!dentry || !dentry->d_inode) return false;
	return S_ISLNK(dentry->d_inode->i_mode);
}

static inline bool dentry_isMountpoint(const struct dentry* dentry) {
	if (!dentry) return false;
	return (dentry->d_flags & DCACHE_MOUNTED) != 0;
}

bool dentry_isEmptyDir(struct dentry* dentry);


/**
 * dentry_allocPath2Mount - 分配并返回dentry相对于其所属挂载点的路径
 * @dentry: 目标dentry
 *
 * 该函数通过向上遍历dentry树，找到挂载点并构建相对路径。
 * 调用者负责使用kfree()释放返回的字符串。
 *
 * 返回值: 成功时返回新分配的相对路径字符串，失败返回NULL
 */
char* dentry_allocPath2Mount(struct dentry* dentry)
{
	#define CONFIG_MAX_PATH_DEPTH 128  /* 定义合理的最大深度 */
    struct dentry* current = dentry;
    struct dentry* mount_point = NULL;
    
    /* 临时存储路径组件的栈 */
    struct {
        const char* name;
        size_t len;
    } path_stack[CONFIG_MAX_PATH_DEPTH];  /* 定义合理的最大深度 */
    
    int stack_pos = 0;
    size_t total_len = 1;  /* 包含起始的'/' */
    char* path, *ptr;
    
    if (!dentry)
        return NULL;
    
    /* 向上遍历查找挂载点 */
    while (current) {
        /* 判断当前dentry是否为挂载点 */
        if (current->d_flags & DCACHE_MOUNTED) {
            mount_point = current;
            break;
        }
        
        /* 如果已经到达文件系统根目录，则该根目录就是挂载点 */
        if (current->d_parent == current || !current->d_parent) {
            mount_point = current;
            break;
        }
        
        /* 将当前dentry名称存入栈中 */
        if (stack_pos < CONFIG_MAX_PATH_DEPTH && current != dentry->d_parent) {
            path_stack[stack_pos].name = current->d_name->name;
            path_stack[stack_pos].len = current->d_name->len;
            total_len += current->d_name->len + 1;  /* +1 是路径分隔符 '/' */
            stack_pos++;
        }
        
        current = current->d_parent;
    }
    
    /* 如果没找到挂载点，返回错误 */
    if (!mount_point)
        return NULL;
    
    /* 分配路径字符串 */
    path = kmalloc(total_len + 1);  /* +1 是字符串结束符 '\0' */
    if (!path)
        return NULL;
    
    /* 构建路径字符串 */
    ptr = path;
    *ptr++ = '/';  /* 起始的根目录标识 */
    
    /* 从栈中按顺序构建路径（反向遍历栈） */
    for (int i = stack_pos - 1; i >= 0; i--) {
        memcpy(ptr, path_stack[i].name, path_stack[i].len);
        ptr += path_stack[i].len;
        
        if (i > 0) {  /* 如果不是最后一个组件，添加分隔符 */
            *ptr++ = '/';
        }
    }
    
    /* 添加字符串结束符 */
    *ptr = '\0';
    
    return path;
}

// 可选：缓存一致性方法
// void d_rehash_subtree(struct dentry *dentry);
// void d_invalidate_subtree(struct dentry *dentry);

// 可选：快照方法
// struct dentry *d_snapshot(struct dentry *dentry);