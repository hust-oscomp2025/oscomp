#ifndef _QSTR_H
#define _QSTR_H

#include <kernel/types.h>

/**
 * Qualified string - string with cached length and hash
 */
struct qstr {
    const unsigned char *name;   /* String content */
    unsigned int len;            /* String length */
    unsigned int hash;           /* Pre-computed hash value */
};

/* Hash algorithm selection */
#define QSTR_HASH_ALGORITHM_FNV1A  0  /* Fowler-Noll-Vo hash */
#define QSTR_HASH_ALGORITHM_DJBX33A 1  /* DJB algorithm */

/* Default hash algorithm */
#define QSTR_DEFAULT_HASH_ALGORITHM QSTR_HASH_ALGORITHM_FNV1A

/* Initialization */
void qstr_init(void);
void qstr_set_hash_algorithm(int algorithm);

/* Core qstr operations */
struct qstr *qstr_create(const char *name);
struct qstr *qstr_create_with_len(const char *name, unsigned int len);
void qstr_free(struct qstr *qstr);
void qstr_init_from_str(struct qstr *qstr, const char *name);
void qstr_init_from_str_with_len(struct qstr *qstr, const char *name, unsigned int len);

/* Hash calculation */
unsigned int qstr_hash(const char *name, unsigned int len);
unsigned int qstr_hash_str(const char *name);
void qstr_update_hash(struct qstr *qstr);

/* Comparison operations */
int qstr_compare(const struct qstr *a, const struct qstr *b);
int qstr_case_compare(const struct qstr *a, const struct qstr *b);
int qstr_prefix_compare(const struct qstr *prefix, const struct qstr *str);

/* Convenient macros */
#define QSTR_INIT(n, l, h) { .name = (n), .len = (l), .hash = (h) }
#define QSTR_LITERAL(s) { .name = (s), .len = sizeof(s)-1, .hash = qstr_hash_str(s) }

/* Advanced operations for filesystem use */
unsigned int full_name_hash(const char *name, unsigned int len);
int qstr_case_eq(const struct qstr *a, const struct qstr *b);
int qstr_eq(const struct qstr *a, const struct qstr *b);

#endif /* _QSTR_H */