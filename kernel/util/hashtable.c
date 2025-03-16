#include <util/hashtable.h>
#include <kernel/mm/kmalloc.h>
#include <util/string.h>
#include <errno.h>

/**
 * Initialize a hash table
 */
int hashtable_init(struct hashtable *ht, 
                  unsigned int initial_size,
                  unsigned int max_load,
                  unsigned int (*hash_func)(const void *key, unsigned int size),
                  int (*key_equals)(const void *key1, const void *key2))
{
    if (!ht || !hash_func || !key_equals)
        return -EINVAL;
    
    /* Default size and load */
    if (initial_size < 16)
        initial_size = 16;
    if (max_load < 50 || max_load > 90)
        max_load = 70;
    
    /* Round to power of 2 */
    initial_size = next_power_of_2(initial_size);
    
    /* Allocate entry array */
    ht->entries = kmalloc(initial_size * sizeof(struct hash_entry));
    if (!ht->entries)
        return -ENOMEM;
    
    /* Initialize entries to empty */
    memset(ht->entries, 0, initial_size * sizeof(struct hash_entry));
    
    /* Initialize table */
    ht->size = initial_size;
    ht->items = 0;
    ht->tombstones = 0;
    ht->max_load = max_load;
    ht->hash_func = hash_func;
    ht->key_equals = key_equals;
    spinlock_init(&ht->lock);
    
    return 0;
}

/**
 * Resize hash table when it gets too full
 * Returns 0 on success, negative on failure
 */
static int hashtable_resize(struct hashtable *ht, unsigned int new_size)
{
    struct hash_entry *old_entries = ht->entries;
    unsigned int old_size = ht->size;
    unsigned int i;
    
    /* Allocate new entry array */
    struct hash_entry *new_entries = kmalloc(new_size * sizeof(struct hash_entry));
    if (!new_entries)
        return -ENOMEM;
    
    /* Initialize new entries */
    memset(new_entries, 0, new_size * sizeof(struct hash_entry));
    
    /* Temporarily replace entries */
    ht->entries = new_entries;
    ht->size = new_size;
    ht->items = 0;
    ht->tombstones = 0;
    
    /* Reinsert all entries */
    for (i = 0; i < old_size; i++) {
        if (old_entries[i].state == HASH_OCCUPIED) {
            hashtable_insert(ht, old_entries[i].key, old_entries[i].value);
        }
    }
    
    /* Free old entries */
    kfree(old_entries);
    
    return 0;
}

/**
 * Insert an item into the hash table
 */
int hashtable_insert(struct hashtable *ht, void *key, void *value)
{
    unsigned int index, i;
    unsigned int mask;
    int err;
    
    if (!ht || !key)
        return -EINVAL;
    
    /* Check if resize is needed */
    if (ht->items + ht->tombstones >= ht->size * ht->max_load / 100) {
        err = hashtable_resize(ht, ht->size * 2);
        if (err < 0)
            return err;
    }
    
    /* Compute initial index */
    mask = ht->size - 1;  /* Fast modulo for power of 2 */
    index = ht->hash_func(key, ht->size) & mask;
    
    /* Linear probing */
    for (i = 0; i < ht->size; i++) {
        unsigned int current = (index + i) & mask;
        
        /* Found an empty or deleted slot */
        if (ht->entries[current].state != HASH_OCCUPIED) {
            ht->entries[current].key = key;
            ht->entries[current].value = value;
            ht->entries[current].state = HASH_OCCUPIED;
            ht->items++;
            
            /* If we used a tombstone, reduce count */
            if (ht->entries[current].state == HASH_DELETED)
                ht->tombstones--;
                
            return 0;
        }
        
        /* Key already exists, update value */
        if (ht->entries[current].state == HASH_OCCUPIED &&
            ht->key_equals(ht->entries[current].key, key)) {
            ht->entries[current].value = value;
            return 0;
        }
    }
    
    /* Table is full - should never happen with proper resizing */
    return -ENOSPC;
}

/**
 * Lookup an item in the hash table
 */
void *hashtable_lookup(struct hashtable *ht, const void *key)
{
    unsigned int index, i;
    unsigned int mask;
    
    if (!ht || !key)
        return NULL;
    
    /* Compute initial index */
    mask = ht->size - 1;
    index = ht->hash_func(key, ht->size) & mask;
    
    /* Linear probing */
    for (i = 0; i < ht->size; i++) {
        unsigned int current = (index + i) & mask;
        
        /* Empty slot means key not found */
        if (ht->entries[current].state == HASH_EMPTY)
            return NULL;
            
        /* Check if key matches */
        if (ht->entries[current].state == HASH_OCCUPIED &&
            ht->key_equals(ht->entries[current].key, key))
            return ht->entries[current].value;
            
        /* Continue on deleted entries */
    }
    
    /* Visited all slots without finding the key */
    return NULL;
}

/**
 * Remove an item from the hash table
 */
int hashtable_remove(struct hashtable *ht, const void *key)
{
    unsigned int index, i;
    unsigned int mask;
    
    if (!ht || !key)
        return -EINVAL;
    
    /* Compute initial index */
    mask = ht->size - 1;
    index = ht->hash_func(key, ht->size) & mask;
    
    /* Linear probing */
    for (i = 0; i < ht->size; i++) {
        unsigned int current = (index + i) & mask;
        
        /* Empty slot means key not found */
        if (ht->entries[current].state == HASH_EMPTY)
            return -ENOENT;
            
        /* Check if key matches */
        if (ht->entries[current].state == HASH_OCCUPIED &&
            ht->key_equals(ht->entries[current].key, key)) {
            /* Mark as deleted */
            ht->entries[current].state = HASH_DELETED;
            ht->entries[current].key = NULL;
            ht->entries[current].value = NULL;
            ht->items--;
            ht->tombstones++;
            
            /* If too many tombstones, resize to clean up */
            if (ht->tombstones > ht->size / 4)
                hashtable_resize(ht, ht->size);
                
            return 0;
        }
    }
    
    /* Key not found */
    return -ENOENT;
}

/**
 * Destroy a hash table
 */
void hashtable_destroy(struct hashtable *ht)
{
    if (!ht)
        return;
        
    if (ht->entries) {
        kfree(ht->entries);
        ht->entries = NULL;
    }
    
    ht->size = 0;
    ht->items = 0;
    ht->tombstones = 0;
}

/**
 * FNV-1a hash function for strings
 */
unsigned int hash_string(const void *key, unsigned int size)
{
    const unsigned char *str = (const unsigned char *)key;
    unsigned int hash = 2166136261u; /* FNV offset basis */
    
    while (*str) {
        hash ^= *str++;
        hash *= 16777619; /* FNV prime */
    }
    
    return hash;
}

/**
 * Simple integer hash
 */
unsigned int hash_int(const void *key, unsigned int size)
{
    unsigned int k = *(const unsigned int *)key;
    k ^= k >> 16;
    k *= 0x85ebca6b;
    k ^= k >> 13;
    k *= 0xc2b2ae35;
    k ^= k >> 16;
    return k;
}

/**
 * Pointer hash - good for inode numbers and similar values
 */
unsigned int hash_ptr(const void *key, unsigned int size)
{
    uintptr_t ptr = (uintptr_t)key;
    return hash_int(&ptr, size);
}