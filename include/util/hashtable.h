#ifndef _HASHTABLE_H
#define _HASHTABLE_H

#include <kernel/types.h>
#include <util/spinlock.h>

/* Hash slot states */
#define HASH_EMPTY      0    /* Empty slot */
#define HASH_OCCUPIED   1    /* Contains an entry */
#define HASH_DELETED    2    /* Previously occupied but now deleted */

/**
 * Hash table entry - kept small for cache efficiency
 */
struct hash_entry {
    void *key;              /* Key pointer */
    void *value;            /* Value pointer */
    uint8_t state;          /* Slot state (EMPTY, OCCUPIED, DELETED) */
};

/**
 * High-performance hash table
 */
struct hashtable {
    /* Configuration */
    unsigned int     size;         /* Number of slots (power of 2) */
    unsigned int     items;        /* Number of items stored */
    unsigned int     tombstones;   /* Number of deleted entries */
    unsigned int     max_load;     /* Maximum load percent (0-100) */
    
    /* Data */
    struct hash_entry *entries;    /* Array of entries */
    
    /* Lock for concurrent access */
    spinlock_t       lock;
    
    /* Operations */
    unsigned int (*hash_func)(const void *key, unsigned int size);
    int (*key_equals)(const void *key1, const void *key2);
};

/**
 * Initialize a hash table
 * @ht: Hash table to initialize
 * @initial_size: Initial number of slots (rounded to next power of 2)
 * @max_load: Maximum load factor percentage (typically 70-80)
 * @hash_func: Hash function
 * @key_equals: Key comparison function
 *
 * Returns: 0 on success, negative error code on failure
 */
int hashtable_init(struct hashtable *ht, 
                  unsigned int initial_size,
                  unsigned int max_load,
                  unsigned int (*hash_func)(const void *key, unsigned int size),
                  int (*key_equals)(const void *key1, const void *key2));

/**
 * Insert an item into the hash table
 * @ht: Hash table
 * @key: Key to insert
 * @value: Value to associate with key
 *
 * Returns: 0 on success, negative error code on failure
 */
int hashtable_insert(struct hashtable *ht, void *key, void *value);

/**
 * Lookup an item in the hash table
 * @ht: Hash table
 * @key: Key to look up
 *
 * Returns: Associated value or NULL if not found
 */
void *hashtable_lookup(struct hashtable *ht, const void *key);

/**
 * Remove an item from the hash table
 * @ht: Hash table
 * @key: Key to remove
 *
 * Returns: 0 on success, negative error code if not found
 */
int hashtable_remove(struct hashtable *ht, const void *key);

/**
 * Destroy a hash table and free its resources
 * @ht: Hash table to destroy
 */
void hashtable_destroy(struct hashtable *ht);

/**
 * Get the number of items in the hash table
 * @ht: Hash table
 *
 * Returns: Number of items stored
 */
static inline unsigned int hashtable_count(struct hashtable *ht) {
    return ht->items;
}

/**
 * Calculate next power of 2
 * @x: Input value
 *
 * Returns: Next power of 2 >= x
 */
static inline unsigned int next_power_of_2(unsigned int x) {
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

/* Standard hash functions */
unsigned int hash_string(const void *key, unsigned int size);
unsigned int hash_int(const void *key, unsigned int size);
unsigned int hash_ptr(const void *key, unsigned int size);

#endif /* _HASHTABLE_H */