#include <kernel/fs/inode.h>
#include <kernel/fs/super_block.h>
#include <kernel/mm/kmalloc.h>
#include <spike_interface/spike_utils.h>
#include <util/hashtable.h>
#include <util/list.h>
#include <util/spinlock.h>

/* Inode cache hashtable */
static struct hashtable inode_hashtable;


/**
 * Hash function for inode keys
 */
static unsigned int inode_hash_func(const void *key, unsigned int size) {
  const struct inode_key *ikey = (const struct inode_key *)key;
  unsigned long val = (unsigned long)ikey->sb ^ ikey->ino;

  /* Mix the bits to distribute values better */
  val = (val * 11400714819323198485ULL) >> 32; /* Fast hash multiply */
  return (unsigned int)val;
}

/**
 * Compare two inode keys for equality
 */
static int inode_key_equals(const void *k1, const void *k2) {
  const struct inode_key *key1 = (const struct inode_key *)k1;
  const struct inode_key *key2 = (const struct inode_key *)k2;

  return (key1->sb == key2->sb && key1->ino == key2->ino);
}

/**
 * inode_cache_init - Initialize the inode cache
 *
 * Sets up the inode cache hash table and related structures.
 */
int inode_cache_init(void) {
  int err;

  sprint("Initializing inode cache\n");

  /* Initialize hash table */
  err = hashtable_init(&inode_hashtable, 1024, 75, inode_hash_func,
                       inode_key_equals);
  if (err != 0) {
    sprint("Failed to initialize inode hashtable: %d\n", err);
    return;
  }

  sprint("Inode cache initialized\n");
}





/**
 * hash_inode - Add an inode to the hash table
 * @inode: The inode to add
 *
 * Adds an inode to the inode hash table for fast lookups.
 */
static void hash_inode(struct inode *inode) {
  struct inode_key *key;

  if (!inode || !inode->i_sb)
    return;

  /* Create hash key */
  key = kmalloc(sizeof(struct inode_key));
  if (!key)
    return;

  key->sb = inode->i_sb;
  key->ino = inode->i_ino;

  /* Insert into hash table */
  hashtable_insert(&inode_hashtable, key, inode);
}

/**
 * unhash_inode - Remove an inode from the hash table
 * @inode: The inode to remove
 */
static void unhash_inode(struct inode *inode) {
  struct inode_key key;

  if (!inode || !inode->i_sb)
    return;

  key.sb = inode->i_sb;
  key.ino = inode->i_ino;

  /* Remove from hash table */
  hashtable_remove(&inode_hashtable, &key);
}



