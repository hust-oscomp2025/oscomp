#include <kernel/mmu.h>
#include <kernel/sched.h>
#include <kernel/util/print.h>
#include <kernel/time.h>
#include <kernel/types.h>
#include <kernel/util.h>
#include <kernel/vfs.h>

/**
 * Initialize the inode cache and hash table
 */
int32 icache_init(void) {
	int32 err;

	sprint("Initializing inode cache\n");

	/* Initialize hash table with our callbacks */
	err = hashtable_setup(&inode_hashtable, 1024, 75, icache_hash, icache_getkey, icache_equal);
	if (err != 0) {
		sprint("Failed to initialize inode hashtable: %d\n", err);
		return err;
	}

	sprint("Inode cache initialized\n");
	return 0;
}



struct inode* icache_lookup(struct superblock* sb, uint64 ino) {
	struct inode* inode;
	struct inode_key key = {.sb = sb, .ino = ino};

	/* Look up in the hash table */
	struct list_node* inode_node = hashtable_lookup(&inode_hashtable, &key);
	CHECK_PTR_VALID(inode_node, NULL);

	inode = container_of(inode_node, struct inode, i_hash_node);
	// inode = hashtable_lookup(&inode_hashtable, &key);

	/* Found in cache - grab a reference */
	inode_ref(inode);
	return inode;
}

/**
 * Hash function for inode keys
 * Uses both superblock pointer and inode number to generate a hash
 */
uint32 icache_hash(const void* key) {
	const struct inode_key* ikey = key;

	/* Combine superblock pointer and inode number */
	uint64 val = (uint64)ikey->sb ^ ikey->ino;

	/* Mix the bits for better distribution */
	val = (val * 11400714819323198485ULL) >> 32; /* Fast hash multiply */
	return (uint32)val;
}


/**
 * icache_insert - Add an inode to the hash table
 * @inode: The inode to add
 *
 * Adds an inode to the inode hash table for fast lookups.
 */
void icache_insert(struct inode* inode) {

	if (!inode || !inode->i_superblock) return;
	/* Insert into hash table */
	hashtable_insert(&inode_hashtable, &inode->i_hash_node);
}

/**
 * icache_delete - Remove an inode from the hash table
 * @inode: The inode to remove
 */
void icache_delete(struct inode* inode) {
	if (!inode || !inode->i_superblock) return;
	/* Remove from hash table */
	hashtable_remove(&inode_hashtable, &inode->i_hash_node);
}