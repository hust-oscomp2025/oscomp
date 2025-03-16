#include <util/qstr.h>
#include <util/string.h>
#include <kernel/mm/kmalloc.h>

/* Current hash algorithm */
static int current_hash_algorithm = QSTR_DEFAULT_HASH_ALGORITHM;

/**
 * qstr_init - Initialize qstr subsystem
 */
void qstr_init(void) 
{
    /* Nothing to do for now */
}

/**
 * qstr_set_hash_algorithm - Change the hash algorithm
 * @algorithm: The algorithm to use
 */
void qstr_set_hash_algorithm(int algorithm)
{
    if (algorithm >= 0 && algorithm <= 1)
        current_hash_algorithm = algorithm;
}

/**
 * fnv1a_hash - FNV-1a hash algorithm
 * @str: String to hash
 * @len: Length of string
 * 
 * Fast, good distribution hash algorithm with minimal collisions.
 */
static unsigned int fnv1a_hash(const unsigned char *str, unsigned int len)
{
    unsigned int hash = 2166136261u; /* FNV offset basis */
    unsigned int i;
    
    for (i = 0; i < len; i++) {
        hash ^= str[i];
        hash *= 16777619; /* FNV prime */
    }
    
    return hash;
}

/**
 * djbx33a_hash - DJBX33A hash algorithm 
 * @str: String to hash
 * @len: Length of string
 *
 * Very fast hash function with good distribution.
 */
static unsigned int djbx33a_hash(const unsigned char *str, unsigned int len)
{
    unsigned int hash = 5381; /* Initial value */
    unsigned int i;
    
    for (i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) ^ str[i]; /* hash * 33 ^ c */
    }
    
    return hash;
}

/**
 * qstr_hash - Hash a string using the current algorithm
 * @name: String to hash
 * @len: Length of string
 */
unsigned int qstr_hash(const char *name, unsigned int len)
{
    if (!name)
        return 0;
        
    switch (current_hash_algorithm) {
        case QSTR_HASH_ALGORITHM_DJBX33A:
            return djbx33a_hash((const unsigned char *)name, len);
        case QSTR_HASH_ALGORITHM_FNV1A:
        default:
            return fnv1a_hash((const unsigned char *)name, len);
    }
}

/**
 * qstr_hash_str - Hash a null-terminated string
 * @name: String to hash
 */
unsigned int qstr_hash_str(const char *name)
{
    if (!name)
        return 0;
    return qstr_hash(name, strlen(name));
}

/**
 * full_name_hash - Alias for qstr_hash for compatibility
 */
unsigned int full_name_hash(const char *name, unsigned int len)
{
    return qstr_hash(name, len);
}

/**
 * qstr_update_hash - Recalculate hash for a qstr
 * @qstr: The qstr to update
 */
void qstr_update_hash(struct qstr *qstr)
{
    if (qstr && qstr->name)
        qstr->hash = qstr_hash((const char *)qstr->name, qstr->len);
}

/**
 * qstr_create - Create a new qstr from a string
 * @name: String to use
 *
 * Returns a new qstr or NULL on allocation failure.
 */
struct qstr *qstr_create(const char *name)
{
    struct qstr *q;
    unsigned int len;
    
    if (!name)
        return NULL;
        
    len = strlen(name);
    
    q = kmalloc(sizeof(struct qstr));
    if (!q)
        return NULL;
        
    q->name = (const unsigned char *)kstrdup(name, GFP_KERNEL);
    if (!q->name) {
        kfree(q);
        return NULL;
    }
    
    q->len = len;
    q->hash = qstr_hash(name, len);
    
    return q;
}

/**
 * qstr_create_with_len - Create a new qstr with explicit length
 * @name: String to use
 * @len: Length of string
 *
 * Returns a new qstr or NULL on allocation failure.
 */
struct qstr *qstr_create_with_len(const char *name, unsigned int len)
{
    struct qstr *q;
    
    if (!name)
        return NULL;
        
    q = kmalloc(sizeof(struct qstr));
    if (!q)
        return NULL;
        
    q->name = (const unsigned char *)kstrndup(name, len, GFP_KERNEL);
    if (!q->name) {
        kfree(q);
        return NULL;
    }
    
    q->len = len;
    q->hash = qstr_hash(name, len);
    
    return q;
}

/**
 * qstr_free - Free a qstr created with qstr_create
 * @qstr: The qstr to free
 */
void qstr_free(struct qstr *qstr)
{
    if (!qstr)
        return;
        
    if (qstr->name)
        kfree((void *)qstr->name);
        
    kfree(qstr);
}

/**
 * qstr_init_from_str - Initialize an existing qstr from a string
 * @qstr: The qstr to initialize
 * @name: String to use
 */
void qstr_init_from_str(struct qstr *qstr, const char *name)
{
    if (!qstr || !name)
        return;
        
    qstr->name = (const unsigned char *)name;
    qstr->len = strlen(name);
    qstr->hash = qstr_hash(name, qstr->len);
}

/**
 * qstr_init_from_str_with_len - Initialize qstr with explicit length
 * @qstr: The qstr to initialize
 * @name: String to use
 * @len: Length of string
 */
void qstr_init_from_str_with_len(struct qstr *qstr, const char *name, unsigned int len)
{
    if (!qstr || !name)
        return;
        
    qstr->name = (const unsigned char *)name;
    qstr->len = len;
    qstr->hash = qstr_hash(name, len);
}

/**
 * qstr_compare - Compare two qstrs
 * @a: First qstr
 * @b: Second qstr
 *
 * Returns -1 if a < b, 0 if a == b, 1 if a > b.
 */
int qstr_compare(const struct qstr *a, const struct qstr *b)
{
    int result;
    
    if (!a || !a->name)
        return b ? -1 : 0;
    if (!b || !b->name)
        return 1;
        
    /* Quick hash comparison first */
    if (a->hash < b->hash)
        return -1;
    if (a->hash > b->hash)
        return 1;
    
    /* Hash match, do string comparison */
    result = memcmp(a->name, b->name, a->len < b->len ? a->len : b->len);
    if (result)
        return result;
        
    /* Strings match up to the length of the shorter one */
    if (a->len < b->len)
        return -1;
    if (a->len > b->len)
        return 1;
        
    return 0;
}

/**
 * qstr_eq - Check if two qstrs are equal
 * @a: First qstr
 * @b: Second qstr
 */
int qstr_eq(const struct qstr *a, const struct qstr *b)
{
    if (!a || !b)
        return 0;
        
    if (a->hash != b->hash)
        return 0;
    if (a->len != b->len)
        return 0;
        
    return memcmp(a->name, b->name, a->len) == 0;
}

/**
 * qstr_case_compare - Compare two qstrs, ignoring case
 * @a: First qstr
 * @b: Second qstr
 */
int qstr_case_compare(const struct qstr *a, const struct qstr *b)
{
    unsigned int i;
    
    if (!a || !a->name)
        return b ? -1 : 0;
    if (!b || !b->name)
        return 1;
    
    /* Compare characters with case folding */
    for (i = 0; i < a->len && i < b->len; i++) {
        unsigned char ca = a->name[i];
        unsigned char cb = b->name[i];
        
        /* Simple ASCII case folding */
        if (ca >= 'A' && ca <= 'Z')
            ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z')
            cb += 'a' - 'A';
            
        if (ca < cb)
            return -1;
        if (ca > cb)
            return 1;
    }
    
    /* Strings match up to the length of the shorter one */
    if (a->len < b->len)
        return -1;
    if (a->len > b->len)
        return 1;
        
    return 0;
}

/**
 * qstr_case_eq - Check if two qstrs are equal, ignoring case
 * @a: First qstr
 * @b: Second qstr
 */
int qstr_case_eq(const struct qstr *a, const struct qstr *b)
{
    unsigned int i;
    
    if (!a || !b)
        return 0;
        
    if (a->len != b->len)
        return 0;
        
    for (i = 0; i < a->len; i++) {
        unsigned char ca = a->name[i];
        unsigned char cb = b->name[i];
        
        /* Simple ASCII case folding */
        if (ca >= 'A' && ca <= 'Z')
            ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z')
            cb += 'a' - 'A';
            
        if (ca != cb)
            return 0;
    }
    
    return 1;
}

/**
 * qstr_prefix_compare - Check if one qstr is a prefix of another
 * @prefix: The prefix qstr
 * @str: The string to check
 */
int qstr_prefix_compare(const struct qstr *prefix, const struct qstr *str)
{
    if (!prefix || !str)
        return 0;
        
    if (prefix->len > str->len)
        return 0;
        
    return memcmp(prefix->name, str->name, prefix->len) == 0;
}