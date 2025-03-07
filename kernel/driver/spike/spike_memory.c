/*
 * scanning the emulated memory from the DTS (Device Tree String).
 * output: the availability and the size (stored in "__uint64_t g_mem_size") of emulated memory.
 *
 * codes are borrowed from riscv-pk (https://github.com/riscv/riscv-pk)
 */
#include <spike_interface/dts_parse.h>
#include <spike_interface/spike_utils.h>
#include <util/string.h>

__uint64_t g_mem_size;

struct mem_scan {
  int memory;
  const __uint32_t *reg_value;
  int reg_len;
};

static void mem_open(const struct fdt_scan_node *node, void *extra) {
  struct mem_scan *scan = (struct mem_scan *)extra;
  memset(scan, 0, sizeof(*scan));
}

static void mem_prop(const struct fdt_scan_prop *prop, void *extra) {
  struct mem_scan *scan = (struct mem_scan *)extra;
  if (!strcmp(prop->name, "device_type") && !strcmp((const char *)prop->value, "memory")) {
    scan->memory = 1;
  } else if (!strcmp(prop->name, "reg")) {
    scan->reg_value = prop->value;
    scan->reg_len = prop->len;
  }
}

static void mem_done(const struct fdt_scan_node *node, void *extra) {
  struct mem_scan *scan = (struct mem_scan *)extra;
  const __uint32_t *value = scan->reg_value;
  const __uint32_t *end = value + scan->reg_len / 4;
  __uint64_t self = (__uint64_t)mem_done;

  if (!scan->memory) return;
  assert(scan->reg_value && scan->reg_len % 4 == 0);

  while (end - value > 0) {
    __uint64_t base, size;
    value = fdt_get_address(node->parent, value, &base);
    value = fdt_get_size(node->parent, value, &size);
    if (base <= self && self <= base + size) {
      g_mem_size = size;
    }
  }
  assert(end == value);
}

// scanning the emulated memory
void query_mem(__uint64_t fdt) {
  struct fdt_cb cb;
  struct mem_scan scan;

  memset(&cb, 0, sizeof(cb));
  cb.open = mem_open;
  cb.prop = mem_prop;
  cb.done = mem_done;
  cb.extra = &scan;

  g_mem_size = 0;
  fdt_scan(fdt, &cb);
  assert(g_mem_size > 0);
}
