#ifndef _RADIX_TREE_H
#define _RADIX_TREE_H

#include <kernel/types.h>
#include <util/spinlock.h>
#include <util/atomic.h>

/*
 * Radix tree node definition.
 * Each node can store up to RADIX_TREE_MAP_SIZE slots.
 */
#define RADIX_TREE_MAP_SHIFT 6
#define RADIX_TREE_MAP_SIZE (1UL << RADIX_TREE_MAP_SHIFT)
#define RADIX_TREE_MAP_MASK (RADIX_TREE_MAP_SIZE - 1)

/* Maximum path height in the radix tree */
#define RADIX_TREE_MAX_HEIGHT 4

/* Node tags for tracking node properties */
#define RADIX_TREE_TAG_DIRTY 0
#define RADIX_TREE_TAG_WRITEBACK 1
#define RADIX_TREE_TAG_ACCESSED 2
#define RADIX_TREE_MAX_TAGS 3

/* Special slot values */
#define RADIX_TREE_EXCEPTIONAL_ENTRY 1
#define RADIX_TREE_EXCEPTIONAL_SHIFT 2

/*
 * Each radix tree node can contain RADIX_TREE_MAP_SIZE slots.
 * For internal nodes, the slots contain pointers to other nodes.
 * For leaf nodes, the slots contain pointers to actual items.
 */
struct radix_tree_node {
    unsigned int height;    /* Height from the leaf level */
    unsigned int count;     /* Number of slots used */
    struct radix_tree_node *parent;   /* Parent node */
    void *slots[RADIX_TREE_MAP_SIZE]; /* Child node/item slots */
    unsigned long tags[RADIX_TREE_MAX_TAGS][RADIX_TREE_MAP_SIZE / (sizeof(unsigned long) * 8)];
};

/*
 * Root structure for radix tree
 */
struct radixTreeRoot {
    unsigned int height;    /* Height of the tree */
    struct radix_tree_node *node;  /* Root node, NULL if tree is empty */
};

/* 
 * Initialization
 */
#define RADIX_TREE_INIT() { 0, NULL }
void radix_tree_init(struct radixTreeRoot *root);

/*
 * Core radix tree operations
 */

/* 
 * Insert an item into the radix tree at the given index
 * Returns 0 on success, -ENOMEM on memory allocation failure,
 * -EEXIST if the slot is already occupied
 */
int radix_tree_insert(struct radixTreeRoot *root, unsigned long index, void *item);

/* 
 * Look up an item in the radix tree at the given index
 * Returns the item if found, NULL if not found
 */
void *radix_tree_lookup(struct radixTreeRoot *root, unsigned long index);

/* 
 * Delete an item from the radix tree at the given index
 * Returns the deleted item if successful, NULL if not found
 */
void *radix_tree_delete(struct radixTreeRoot *root, unsigned long index);

/*
 * Gang lookup - find multiple items starting at a given index
 * Fills the results array with up to max_items matching items
 * Returns the number of items found
 */
unsigned int radix_tree_gang_lookup(struct radixTreeRoot *root,
        void **results, unsigned long first_index, unsigned int max_items);

/*
 * Tag operations
 */

/* Set a tag on an item */
int radix_tree_tag_set(struct radixTreeRoot *root, unsigned long index, unsigned int tag);

/* Clear a tag from an item */
int radix_tree_tag_clear(struct radixTreeRoot *root, unsigned long index, unsigned int tag);

/* Check if an item has a tag set */
int radix_tree_tag_get(struct radixTreeRoot *root, unsigned long index, unsigned int tag);

/* Gang lookup for tagged items */
unsigned int radix_tree_gang_lookup_tag(struct radixTreeRoot *root,
        void **results, unsigned long first_index, unsigned int max_items, 
        unsigned int tag);

/*
 * Tree operations
 */

/* Destroy (free) all nodes in the tree */
void radix_tree_destroy(struct radixTreeRoot *root);

/* Count items in the tree */
unsigned long radix_tree_count_items(struct radixTreeRoot *root);

/* Apply a function to each item in the tree */
unsigned int radix_tree_for_each(struct radixTreeRoot *root,
        int (*fn)(void *item, unsigned long index, void *data), void *data);

#endif /* _RADIX_TREE_H */