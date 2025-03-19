#ifndef _DENTRY_H
#define _DENTRY_H

#include <kernel/fs/vfs.h>
#include <kernel/types.h>
#include <util/list.h>
#include <util/atomic.h>
#include <util/spinlock.h>
#include <util/qstr.h>
#include <kernel/fs/inode.h>

/* Forward declarations */
struct inode;
struct super_block;
struct nameidata;
struct path;
struct dentry_operations;
/*
 * Directory entry (dentry) structure
 * 
 * A dentry is the glue that connects inodes and paths. Each path component
 * (directory or file name) is represented by a dentry.
 */
struct dentry {
    spinlock_t d_lock;              /* Protects dentry fields */
    atomic_t d_refcount;               /* Reference count */


    /* RCU lookup touched fields */
    unsigned int d_flags;           /* Dentry flags */
    struct inode *d_inode;          /* Associated inode */
    
    /* Lookup cache information */
    struct qstr d_name;             /* Name of this dentry */
    struct list_node d_hashNode;    /* Lookup hash list */

    struct dentry *d_parent;        /* Parent dentry */
    struct list_node d_parentListNode;       /* Child entry in parent's list */
    
    /* Linked list of child dentries */
    struct list_head d_childList;     /* Child dentries */
    
    /* Dentry usage management */
    
    /* Filesystem and operations */
    struct super_block *d_superblock;       /* Superblock of file */
    
    /* D-cache management */
    unsigned long d_time;           /* Revalidation time */
    void *d_fsdata;                 /* Filesystem-specific data */
    
    struct list_node d_lruListNode;         /* 全局dentry的LRU链表，在引用计数归零时加入，便于复用 */
									/* 只在内存压力时释放*/
    struct list_node d_aliasListNode;   /* Inode 别名链表，用来维护硬链接 */

    /* Mount management */
    int d_mounted;                  /* Mount count */
    struct path *d_automount;       /* Automount point */

    const struct dentry_operations *d_operations;  /* Dentry operations */

};

int init_dentry_hashtable(void);

/*dentry lru链表相关*/
void init_dentry_lruList(void);
unsigned int shrink_dentry_lru(unsigned int count);

/*dentry生命周期*/
struct dentry* dentry_get(struct dentry* parent, const struct qstr* name, 
	int is_dir, bool revalidate, bool alloc);
int dentry_put(struct dentry *dentry);

int dentry_set_inode(struct dentry *dentry, struct inode *inode);







/* Lookup operations */
int d_validate(struct dentry *dentry, struct dentry *dparent);

/* Path operations */
char *dentry_path_raw(struct dentry *dentry, char *buf, int buflen);
char *dentry_path(struct dentry *dentry, char *buf, int buflen);
struct dentry *d_splice_alias(struct inode *inode, struct dentry *dentry);
struct dentry *d_make_root(struct inode *root_inode);

/* Management operations */
void d_move(struct dentry *dentry, struct dentry *target);
void d_rehash(struct dentry *dentry);
void d_add(struct dentry *dentry, struct inode *inode);
struct dentry *d_obtain_alias(struct inode *inode);

/* Cache management */
void shrink_dcache_parent(struct dentry *parent);
void shrink_dcache_sb(struct super_block *sb);
void prune_dcache_sb(struct super_block *sb);

/*
 * Helpers for name handling
 */
int dname_compare(const char *name1, int len1, const char *name2, int len2);
unsigned int full_name_hash(const unsigned char *name, unsigned int len);
void qstr_init(struct qstr *qstr, const char *name, unsigned int len);

/*
 * Mount point enumeration
 */
bool is_mounted(struct dentry *dentry);
void d_flags_for_inode(struct inode *inode);

/*
 * Pathname traversal
 */
struct dentry *kern_path_locked(const char *name, struct path *path);
struct dentry *lookup_one_len(const char *name, struct dentry *base, int len);


/* 
 * Operations that can be specialized for a particular dentry
 */
struct dentry_operations {
    /* Called to determine if dentry is still valid - important for NFS etc */
    int (*d_revalidate)(struct dentry *, unsigned int);
    
    /* Called to hash the dentry name for dcache */
    int (*d_hash)(const struct dentry *, struct qstr *);
    
    /* Called for name comparisons */
    int (*d_compare)(const struct dentry *, unsigned int, const char *, const struct qstr *);
    
    /* Called when dentry's reference count reaches 0 */
    int (*d_free)(const struct dentry *);
    
    /* Called to release dentry's inode */
    void (*d_put_inode)(struct dentry *, struct inode *);
    
    /* Called to create the relative path of a dentry */
    char *(*d_dname)(struct dentry *, char *, int);
    
    /* Called when a dentry is unhashed */
    void (*d_prune)(struct dentry *);
};

/*
 * Dentry state flags
 */
#define DCACHE_DISCONNECTED     0x0001  /* Dentry is disconnected from the FS tree */
#define DCACHE_OP_HASH          0x0002  /* Has a custom hash operation */
#define DCACHE_OP_COMPARE       0x0004  /* Has a custom compare operation */
#define DCACHE_OP_REVALIDATE    0x0008  /* Has a revalidate operation */
#define DCACHE_OP_DELETE        0x0010  /* Has a delete operation */
#define DCACHE_REFERENCED       0x0040  /* Recently used */
#define DCACHE_RCUACCESS        0x0080  /* RCU-enabled */
#define DCACHE_NEGATIVE         0x0100  /* Negative dentry (no inode exists) */
#define DCACHE_FALLTHRU         0x0200  /* Used for lookup fallthrough */
#define DCACHE_NEED_AUTOMOUNT   0x1000  /* Automount point */
#define DCACHE_MOUNTED          0x2000  /* Is a mountpoint */
#define DCACHE_HASHED           0x4000  /* In dentry hash table */



#endif /* _DENTRY_H */