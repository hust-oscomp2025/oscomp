#ifndef _DT_PARSE_H_
#define _DT_PARSE_H_

#include <sys/types.h>
#include <kernel/types.h>
#include <kernel/util/print.h>

#define FDT_MAGIC 0xd00dfeed
#define FDT_VERSION 17

struct fdt_header {
  __uint32_t magic;
  __uint32_t totalsize;
  __uint32_t off_dt_struct;
  __uint32_t off_dt_strings;
  __uint32_t off_mem_rsvmap;
  __uint32_t version;
  __uint32_t last_comp_version; /* <= 17 */
  __uint32_t boot_cpuid_phys;
  __uint32_t size_dt_strings;
  __uint32_t size_dt_struct;
};

#define FDT_BEGIN_NODE 1
#define FDT_END_NODE 2
#define FDT_PROP 3
#define FDT_NOP 4
#define FDT_END 9

struct fdt_scan_node {
  const struct fdt_scan_node *parent;
  const char *name;
  int32 address_cells;
  int32 size_cells;
};

struct fdt_scan_prop {
  const struct fdt_scan_node *node;
  const char *name;
  __uint32_t *value;
  int32 len;  // in bytes of value
};

struct fdt_cb {
  void (*open)(const struct fdt_scan_node *node, void *extra);
  void (*prop)(const struct fdt_scan_prop *prop, void *extra);
  void (*done)(const struct fdt_scan_node *node,
               void *extra);  // last property was seen
  int32 (*close)(const struct fdt_scan_node *node,
               void *extra);  // -1 => delete the node + children
  void *extra;
};

// Scan the contents of FDT
void fdt_scan(__uint64_t fdt, const struct fdt_cb *cb);
__uint32_t fdt_size(__uint64_t fdt);

// Extract fields
const __uint32_t *fdt_get_address(const struct fdt_scan_node *node, const __uint32_t *base, __uint64_t *value);
const __uint32_t *fdt_get_size(const struct fdt_scan_node *node, const __uint32_t *base, __uint64_t *value);
int32 fdt_string_list_index(const struct fdt_scan_prop *prop,
                          const char *str);  // -1 if not found
#endif
