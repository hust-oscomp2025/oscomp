#ifndef _QSTR_H
#define _QSTR_H

#include <kernel/types.h>

/**
 * Qualified string - string with cached length and hash
 */
struct qstr {
    const char *name;   /* String content */
    uint32 len;            /* String length */
    uint32 hash;           /* Pre-computed hash value */
};

/* Hash algorithm selection */
#define QSTR_HASH_ALGORITHM_FNV1A  0  /* Fowler-Noll-Vo hash */
#define QSTR_HASH_ALGORITHM_DJBX33A 1  /* DJB algorithm */

/* Default hash algorithm */
#define QSTR_DEFAULT_HASH_ALGORITHM QSTR_HASH_ALGORITHM_FNV1A

/* Initialization */
void qstr_init(void);
void qstr_set_hash_algorithm(int32 algorithm);

/* Core qstr s_operations */
struct qstr *qstr_create(const char *name);
struct qstr *qstr_create_with_length(const char *name, uint32 len);
void qstr_free(struct qstr *qstr);
void qstr_init_from_str(struct qstr *qstr, const char *name);
void qstr_init_from_str_with_len(struct qstr *qstr, const char *name, uint32 len);

/* Hash calculation */
uint32 qstr_hash(const char *name, uint32 len);
uint32 qstr_hash_str(const char *name);
void qstr_update_hash(struct qstr *qstr);

/* Comparison s_operations */
int32 qstr_compare(const struct qstr *a, const struct qstr *b);
int32 qstr_case_compare(const struct qstr *a, const struct qstr *b);
int32 qstr_prefix_compare(const struct qstr *prefix, const struct qstr *str);

/* Convenient macros */
#define QSTR_INIT(n, l, h) { .name = (n), .len = (l), .hash = (h) }
#define QSTR_LITERAL(s) { .name = (s), .len = sizeof(s)-1, .hash = qstr_hash_str(s) }

/* Advanced s_operations for filesystem use */
uint32 full_name_hash(const char *name, uint32 len);
int32 qstr_case_eq(const struct qstr *a, const struct qstr *b);
int32 qstr_eq(const struct qstr *a, const struct qstr *b);

#endif /* _QSTR_H */