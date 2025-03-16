#include <kernel/fs/inode.h>

static void inode_init_once(struct inode *inode);

/**
 * new_inode - Allocate and initialize a new inode
 * @sb: The superblock for this inode
 *
 * Allocates a new inode and initializes it for the given superblock.
 * Returns the new inode or NULL on failure.
 */
struct inode *new_inode(struct super_block *sb) {
  struct inode *inode;

  if (!sb)
    return NULL;

  /* Allocate the inode */
  inode = kmalloc(sizeof(struct inode));
  if (!inode)
    return NULL;

  /* Initialize it */
  memset(inode, 0, sizeof(struct inode));
  inode_init_once(inode);

  /* Set basic inode fields */
  inode->i_sb = sb;
  inode->i_state = I_NEW;         /* Mark as new */
  atomic_set(&inode->i_count, 1); /* Initial reference */

  /* Add to superblock's inode list */
  list_add(&inode->i_sb_list_node, &sb->s_inode_list);

  return inode;
}




/**
 * put_inode - Release a reference to an inode
 * @inode: The inode to release
 *
 * Decrements the reference count on an inode and performs cleanup
 * when the count reaches zero.
 */
void put_inode(struct inode *inode) {
  struct super_block *sb = inode->i_sb;
	spinlock_lock(&inode->i_lock);
  /* Decrease reference count */
  if (atomic_dec_and_test(&inode->i_count)) {
    /* Last reference gone - add to superblock's LRU */
    if (!(inode->i_state & I_DIRTY)) {
      /* If it's on another state list, remove it first */
      spin_lock(&sb->s_inode_states_lock);
      if (!list_empty(&inode->i_state_list_node)) {
        list_del_init(&inode->i_state_list_node);
      }

      list_add_tail(&inode->i_state_list_node, &sb->s_inode_lru_list);
      spin_unlock(&sb->s_inode_states_lock);
    }
  }
	spinlock_unlock(&inode->i_lock);
}

/**
 * mark_inode_dirty - Mark an inode as needing to be written to disk
 * @inode: The inode to mark
 */
void mark_inode_dirty(struct inode *inode) {
  struct super_block *sb = inode->i_sb;
	spinlock_lock(&inode->i_lock);

  spinlock_lock(&sb->s_inode_states_lock);

  /* If it's on another state list, remove it first */
  if (!list_empty(&inode->i_state_list_node))
    list_del_init(&inode->i_state_list_node);

  /* Add to dirty list */
  list_add_tail(&inode->i_state_list_node, &sb->s_inode_dirty_list);

  /* Update state */
  inode->i_state |= I_DIRTY;

  spinlock_unlock(&sb->s_inode_states_lock);

	spinlock_unlock(&inode->i_lock);

}


/**
 * iget - Get an inode from the inode cache
 * @sb: Superblock
 * @ino: Inode number
 *
 * Looks up an inode in the cache. If found, increments its reference count
 * and returns it. If not found, returns NULL.
 */
struct inode *iget(struct super_block *sb, unsigned long ino) {
  struct inode_key key;
  struct inode *inode;

  if (!sb)
    return NULL;

  /* Create lookup key */
  key.sb = sb;
  key.ino = ino;

  /* Look up in hash table */
  inode = hashtable_lookup(&inode_hashtable, &key);

  if (inode) {
    /* Found in cache - increase reference count */
    atomic_inc(&inode->i_count);
  }

  return inode;
}





/**
 * inode_init_once - Initialize a newly allocated inode
 * @inode: The inode to initialize
 *
 * Sets up the initial state of an inode.
 */
static void inode_init_once(struct inode *inode) {
  if (!inode)
    return;

  /* Initialize reference count */
  atomic_set(&inode->i_count, 0);

  /* Initialize locks */
  spinlock_init(&inode->i_lock);

  /* Initialize lists */
  INIT_LIST_HEAD(&inode->i_sb_list_node);
	INIT_LIST_HEAD(&inode->i_state_list_node);
  INIT_LIST_HEAD(&inode->i_dentry);

  /* Initialize state */
  inode->i_state = 0;
}