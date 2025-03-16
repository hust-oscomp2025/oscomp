#include <kernel/fs/dentry.h>
#include <util/hashtable.h>

/* Dentry cache hashtable */
static struct hashtable dentry_cache;

/* Compare dentry names */
static int dentry_name_equals(const void *k1, const void *k2)
{
    const struct qstr *name1 = (const struct qstr *)k1;
    const struct qstr *name2 = (const struct qstr *)k2;
    
    if (name1->len != name2->len)
        return 0;
        
    return !memcmp(name1->name, name2->name, name1->len);
}

/* Hash a dentry name */
static unsigned int dentry_name_hash(const void *key, unsigned int size)
{
    const struct qstr *name = (const struct qstr *)key;
    return name->hash;
}

/**
 * d_cache_init - Initialize the dentry cache
 */
int d_cache_init(void)
{
    sprint("Initializing dentry cache\n");
    
    /* Initialize dentry hashtable */
    return hashtable_init(&dentry_cache, 1024, 75, 
                         dentry_name_hash, dentry_name_equals);
}