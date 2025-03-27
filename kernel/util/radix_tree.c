#include <kernel/mm/kmalloc.h>
#include <kernel/util/radix_tree.h>
#include <errno.h>

/*
 * Private utility functions
 */

/* Allocate a new radix tree node */
static struct radix_tree_node *radix_tree_node_alloc(void)
{
    struct radix_tree_node *node;
    
    node = kzalloc(sizeof(struct radix_tree_node));
    if (node) {
        node->count = 0;
        node->parent = NULL;
    }
    
    return node;
}

/* Free a radix tree node */
static void radix_tree_node_free(struct radix_tree_node *node)
{
    if (node)
        kfree(node);
}

/* Calculate the index of a slot in a particular level of the tree */
static uint32 radix_tree_index_in_node(uint64 index, uint32 height)
{
    uint32 shift = (height - 1) * RADIX_TREE_MAP_SHIFT;
    return (index >> shift) & RADIX_TREE_MAP_MASK;
}

/* Extend the tree height to accommodate a particular index */
static int32 radix_tree_extend(struct radixTreeRoot *root, uint64 index)
{
    struct radix_tree_node *node;
    uint32 height;
    int32 result;

    /* Calculate the height needed for this index */
    height = root->height;
    while (index > (1UL << (height * RADIX_TREE_MAP_SHIFT))) {
        if (height >= RADIX_TREE_MAX_HEIGHT)
            return -EINVAL;  /* Index too large for the tree */
        height++;
    }

    if (height == root->height)
        return 0;  /* No extension needed */

    /* Extend the tree by adding new levels */
    do {
        if (root->height == 0) {
            /* Empty tree case */
            node = radix_tree_node_alloc();
            if (!node)
                return -ENOMEM;
            root->node = node;
            root->height = 1;
        } else {
            /* Add a new root node */
            node = radix_tree_node_alloc();
            if (!node)
                return -ENOMEM;
            
            /* Link in the existing tree */
            node->slots[0] = root->node;
            node->height = root->height + 1;
            node->count = 1;
            if (root->node)
                ((struct radix_tree_node *)root->node)->parent = node;
            
            root->node = node;
            root->height++;
        }
    } while (height > root->height);

    return 0;
}

/* Create a path to a leaf node for a given index */
static int32 radix_tree_create_path(struct radixTreeRoot *root, uint64 index,
                                 void **slot_ptr)
{
    struct radix_tree_node *node, *tmp, *child;
    uint32 i, height, shift, offset;
    int32 result;

    /* Extend the tree if necessary */
    result = radix_tree_extend(root, index);
    if (result < 0)
        return result;

    /* Start from the root and descend to the leaf */
    node = root->node;
    height = root->height;
    shift = (height - 1) * RADIX_TREE_MAP_SHIFT;

    for (i = 0; i < height - 1; i++) {
        if (node == NULL) {
            /* This should never happen if extend worked correctly */
            return -EINVAL;
        }

        /* Get index for this level of the tree */
        offset = (index >> shift) & RADIX_TREE_MAP_MASK;
        
        child = node->slots[offset];
        if (!child) {
            /* Need to create a child node */
            child = radix_tree_node_alloc();
            if (!child)
                return -ENOMEM;
            
            child->height = height - (i + 1);
            child->parent = node;
            node->slots[offset] = child;
            node->count++;
        }
        
        node = child;
        shift -= RADIX_TREE_MAP_SHIFT;
    }

    /* Now node is the leaf node, set the slot pointer */
    offset = index & RADIX_TREE_MAP_MASK;
    *slot_ptr = &node->slots[offset];
    
    return 0;
}

/* Check if a node can be deleted (has no items or child nodes) */
static int32 radix_tree_node_can_delete(struct radix_tree_node *node)
{
    return node && node->count == 0;
}

/* Delete nodes recursively upward if empty */
static struct radix_tree_node *radix_tree_delete_node(struct radixTreeRoot *root,
                                                    struct radix_tree_node *node,
                                                    uint32 height,
                                                    uint64 index)
{
    uint32 offset = radix_tree_index_in_node(index, height);
    struct radix_tree_node *child = node->slots[offset];
    struct radix_tree_node *parent;
    
    if (height > 1) {
        /* Internal node case */
        child = radix_tree_delete_node(root, child, height - 1, index);
        
        /* Update the child pointer, might be NULL if child was deleted */
        node->slots[offset] = child;
        if (!child)
            node->count--;
    } else {
        /* Leaf node case, just clear the slot */
        node->slots[offset] = NULL;
        node->count--;
    }
    
    /* Check if this node is now empty and can be deleted */
    if (radix_tree_node_can_delete(node)) {
        parent = node->parent;
        radix_tree_node_free(node);
        return NULL;
    }
    
    return node;
}

/* Helper to shrink the tree height if possible */
static void radix_tree_shrink(struct radixTreeRoot *root)
{
    struct radix_tree_node *node, *child;
    
    node = root->node;
    
    /* If height <= 1 or root has multiple children, we can't shrink */
    while (root->height > 1 && node && node->count == 1) {
        child = node->slots[0];
        
        /* If this isn't a node, we're done */
        if (!child || !(child->height == node->height - 1))
            break;
        
        /* Replace the root with the child */
        root->node = child;
        root->height--;
        
        /* Free the old root */
        child->parent = NULL;
        radix_tree_node_free(node);
        
        node = child;
    }
    
    /* If the tree is now empty, reset the height */
    if (root->height > 0 && !root->node)
        root->height = 0;
}

/*
 * Public API implementations
 */

/* Initialize a radix tree */
void radix_tree_init(struct radixTreeRoot *root)
{
    root->height = 0;
    root->node = NULL;
}

/* Insert an item into the radix tree */
int32 radix_tree_insert(struct radixTreeRoot *root, uint64 index, void *item)
{
    void **slot;
    int32 result;
    
    /* Cannot insert NULL items */
    if (!item)
        return -EINVAL;
    
    /* Create a path to the insertion point */
    result = radix_tree_create_path(root, index, &slot);
    if (result < 0)
        return result;
    
    /* Check if the slot is already occupied */
    if (*slot)
        return -EEXIST;
    
    /* Store the item */
    *slot = item;
    
    return 0;
}

/* Look up an item in the radix tree */
void *radix_tree_lookup(struct radixTreeRoot *root, uint64 index)
{
    struct radix_tree_node *node;
    uint32 height, shift, offset;
    
    if (!root->height)
        return NULL;
    
    /* Check if the index is beyond tree height */
    if (index > (1UL << (root->height * RADIX_TREE_MAP_SHIFT)))
        return NULL;
    
    node = root->node;
    height = root->height;
    shift = (height - 1) * RADIX_TREE_MAP_SHIFT;
    
    while (height > 0) {
        if (!node)
            return NULL;
        
        offset = (index >> shift) & RADIX_TREE_MAP_MASK;
        node = node->slots[offset];
        
        height--;
        shift -= RADIX_TREE_MAP_SHIFT;
    }
    
    return node;  /* At leaf level, node is actually the item */
}

/* Delete an item from the radix tree */
void *radix_tree_delete(struct radixTreeRoot *root, uint64 index)
{
    struct radix_tree_node *node;
    uint32 height, shift, offset;
    void *item = NULL;
    
    if (!root->height)
        return NULL;
    
    /* Check if the index is beyond tree height */
    if (index > (1UL << (root->height * RADIX_TREE_MAP_SHIFT)))
        return NULL;
    
    /* Special case for single-level trees */
    if (root->height == 1) {
        node = root->node;
        offset = index & RADIX_TREE_MAP_MASK;
        
        if (node && offset < RADIX_TREE_MAP_SIZE) {
            item = node->slots[offset];
            if (item) {
                node->slots[offset] = NULL;
                node->count--;
                
                if (node->count == 0) {
                    radix_tree_node_free(node);
                    root->node = NULL;
                    root->height = 0;
                }
            }
        }
        return item;
    }
    
    /* For multi-level trees, find the leaf node */
    node = root->node;
    height = root->height;
    shift = (height - 1) * RADIX_TREE_MAP_SHIFT;
    
    /* Traverse to the parent of the leaf */
    while (height > 1) {
        if (!node)
            return NULL;
        
        offset = (index >> shift) & RADIX_TREE_MAP_MASK;
        node = node->slots[offset];
        
        height--;
        shift -= RADIX_TREE_MAP_SHIFT;
    }
    
    /* Now we're at the parent of the leaf */
    if (!node)
        return NULL;
    
    offset = index & RADIX_TREE_MAP_MASK;
    item = node->slots[offset];
    
    if (item) {
        node->slots[offset] = NULL;
        node->count--;
        
        /* Clean up empty nodes */
        if (root->height > 1)
            radix_tree_delete_node(root, root->node, root->height, index);
            
        /* Shrink the tree if possible */
        radix_tree_shrink(root);
    }
    
    return item;
}

/* Gang lookup - find multiple items */
uint32 radix_tree_gang_lookup(struct radixTreeRoot *root,
                                    void **results, uint64 first_index,
                                    uint32 max_items)
{
    uint64 index = first_index;
    uint32 found = 0;
    void *item;
    
    while (found < max_items) {
        item = radix_tree_lookup(root, index);
        if (!item)
            break;
        
        results[found] = item;
        found++;
        index++;
        
        /* Detect index wrap-around */
        if (index < first_index)
            break;
    }
    
    return found;
}

/* Set a tag on an item */
int32 radix_tree_tag_set(struct radixTreeRoot *root, uint64 index, uint32 tag)
{
    struct radix_tree_node *node, *parent;
    uint32 height, shift, offset;
    
    if (tag >= RADIX_TREE_MAX_TAGS)
        return -EINVAL;
    
    if (!root->height)
        return -ENOENT;
    
    /* Check if the index is beyond tree height */
    if (index > (1UL << (root->height * RADIX_TREE_MAP_SHIFT)))
        return -ENOENT;
    
    node = root->node;
    height = root->height;
    shift = (height - 1) * RADIX_TREE_MAP_SHIFT;
    
    /* Walk down to the leaf node */
    while (height > 0) {
        if (!node)
            return -ENOENT;
        
        offset = (index >> shift) & RADIX_TREE_MAP_MASK;
        
        /* Set the tag at this level */
        node->tags[tag][offset / (sizeof(uint64) * 8)] |= 
            1UL << (offset % (sizeof(uint64) * 8));
        
        /* Move to the next level */
        node = node->slots[offset];
        height--;
        shift -= RADIX_TREE_MAP_SHIFT;
    }
    
    return 0;
}

/* Clear a tag from an item */
int32 radix_tree_tag_clear(struct radixTreeRoot *root, uint64 index, uint32 tag)
{
    struct radix_tree_node *node, *parent;
    uint32 height, shift, offset;
    uint64 tag_bit;
    
    if (tag >= RADIX_TREE_MAX_TAGS)
        return -EINVAL;
    
    if (!root->height)
        return -ENOENT;
    
    /* Check if the index is beyond tree height */
    if (index > (1UL << (root->height * RADIX_TREE_MAP_SHIFT)))
        return -ENOENT;
    
    node = root->node;
    height = root->height;
    shift = (height - 1) * RADIX_TREE_MAP_SHIFT;
    
    /* Walk down to the leaf node while clearing tags */
    while (height > 0) {
        if (!node)
            return -ENOENT;
        
        offset = (index >> shift) & RADIX_TREE_MAP_MASK;
        
        /* Calculate bit position in the tag word */
        tag_bit = 1UL << (offset % (sizeof(uint64) * 8));
        
        /* Clear the tag at this level */
        node->tags[tag][offset / (sizeof(uint64) * 8)] &= ~tag_bit;
        
        /* Move to the next level */
        node = node->slots[offset];
        height--;
        shift -= RADIX_TREE_MAP_SHIFT;
    }
    
    return 0;
}

/* Check if an item has a tag set */
int32 radix_tree_tag_get(struct radixTreeRoot *root, uint64 index, uint32 tag)
{
    struct radix_tree_node *node;
    uint32 height, shift, offset;
    uint64 tag_bit;
    
    if (tag >= RADIX_TREE_MAX_TAGS)
        return 0;
    
    if (!root->height)
        return 0;
    
    /* Check if the index is beyond tree height */
    if (index > (1UL << (root->height * RADIX_TREE_MAP_SHIFT)))
        return 0;
    
    node = root->node;
    height = root->height;
    shift = (height - 1) * RADIX_TREE_MAP_SHIFT;
    
    while (height > 0) {
        if (!node)
            return 0;
        
        offset = (index >> shift) & RADIX_TREE_MAP_MASK;
        
        /* Calculate bit position in the tag word */
        tag_bit = 1UL << (offset % (sizeof(uint64) * 8));
        
        /* Check if tag is set at this level */
        if (!(node->tags[tag][offset / (sizeof(uint64) * 8)] & tag_bit))
            return 0;
        
        /* Move to the next level */
        node = node->slots[offset];
        height--;
        shift -= RADIX_TREE_MAP_SHIFT;
    }
    
    return 1;  /* Tag is set for this item */
}

/* Gang lookup for tagged items */
uint32 radix_tree_gang_lookup_tag(struct radixTreeRoot *root,
                                        void **results, uint64 first_index,
                                        uint32 max_items, uint32 tag)
{
    uint64 index = first_index;
    uint32 found = 0;
    void *item;
    
    if (tag >= RADIX_TREE_MAX_TAGS)
        return 0;
    
    while (found < max_items) {
        /* Skip entries until we find a tagged one */
        while (!radix_tree_tag_get(root, index, tag)) {
            index++;
            if (index < first_index)  /* Detect wrap-around */
                return found;
        }
        
        item = radix_tree_lookup(root, index);
        if (!item)
            break;
        
        results[found] = item;
        found++;
        index++;
        
        /* Detect index wrap-around */
        if (index < first_index)
            break;
    }
    
    return found;
}

/* Helper for radix_tree_destroy */
static void radix_tree_destroy_node(struct radix_tree_node *node, uint32 height)
{
    uint32 i;
    
    if (!node)
        return;
    
    if (height > 1) {
        /* Recursively destroy child nodes */
        for (i = 0; i < RADIX_TREE_MAP_SIZE; i++) {
            if (node->slots[i])
                radix_tree_destroy_node(node->slots[i], height - 1);
        }
    }
    
    radix_tree_node_free(node);
}

/* Destroy the entire radix tree */
void radix_tree_destroy(struct radixTreeRoot *root)
{
    if (root->height > 0 && root->node) {
        radix_tree_destroy_node(root->node, root->height);
        root->height = 0;
        root->node = NULL;
    }
}

/* Helper for radix_tree_count_items */
static uint64 _count_items(struct radix_tree_node *node, uint32 height)
{
    uint64 count = 0;
    uint32 i;
    
    if (!node)
        return 0;
    
    if (height == 1) {
        /* Leaf node - count non-NULL slots */
        for (i = 0; i < RADIX_TREE_MAP_SIZE; i++) {
            if (node->slots[i])
                count++;
        }
    } else {
        /* Internal node - recurse and sum */
        for (i = 0; i < RADIX_TREE_MAP_SIZE; i++) {
            if (node->slots[i])
                count += _count_items(node->slots[i], height - 1);
        }
    }
    
    return count;
}

/* Count items in the tree */
uint64 radix_tree_count_items(struct radixTreeRoot *root)
{
    if (!root->height || !root->node)
        return 0;
    
    return _count_items(root->node, root->height);
}

/* Helper for radix_tree_for_each */
static uint32 _for_each(struct radix_tree_node *node, uint32 height, 
                            uint64 base_index, int32 (*fn)(void *, uint64, void *), 
                            void *data)
{
    uint32 i, count = 0;
    uint64 index;
    
    if (!node)
        return 0;
    
    if (height == 1) {
        /* Leaf node - apply function to items */
        for (i = 0; i < RADIX_TREE_MAP_SIZE; i++) {
            if (node->slots[i]) {
                index = base_index | i;
                if (fn(node->slots[i], index, data) == 0)
                    count++;
                else
                    return count;  /* Stop if function returns non-zero */
            }
        }
    } else {
        /* Internal node - recurse */
        for (i = 0; i < RADIX_TREE_MAP_SIZE; i++) {
            if (node->slots[i]) {
                uint64 new_base = base_index | (i << ((height - 1) * RADIX_TREE_MAP_SHIFT));
                uint32 ret = _for_each(node->slots[i], height - 1, new_base, fn, data);
                count += ret;
                if (ret != _count_items(node->slots[i], height - 1))
                    return count;  /* Stop if function returned non-zero */
            }
        }
    }
    
    return count;
}

/* Apply a function to each item in the tree */
uint32 radix_tree_for_each(struct radixTreeRoot *root,
                                int32 (*fn)(void *item, uint64 index, void *data),
                                void *data)
{
    if (!root->height || !root->node)
        return 0;
    
    return _for_each(root->node, root->height, 0, fn, data);
}