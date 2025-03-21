/*
 * HTIF (Host-Target InterFace) scanning. 
 * output: the availability of HTIF (indicated by "__uint64_t htif")
 *
 * HTIF is a powerful utility provided by the underlying emulator, i.e., Spike.
 * with HTIF, target environment (i.e., the RISC-V machine we use) can leverage 
 * the power (e.g., read spike_files, print strings to screen and many others) of host 
 * at ease (by issueing HTIF syscalls to the hosts via htif_syscall).
 *
 * codes are borrowed from riscv-pk (https://github.com/riscv/riscv-pk)
 */

#include <sys/types.h>
#include <util/spinlock.h>
#include <spike_interface/spike_htif.h>
#include <spike_interface/spike_utils.h>
#include <spike_interface/dts_parse.h>
#include <util/string.h>

__uint64_t htif;  //is Spike HTIF avaiable? initially 0 (false)

///////////////////////////    Spike HTIF discovering    //////////////////////////////
struct htif_scan {
  int compat;
};

static void htif_open(const struct fdt_scan_node *node, void *extra) {
  struct htif_scan *scan = (struct htif_scan *)extra;
  memset(scan, 0, sizeof(*scan));
}

static void htif_prop(const struct fdt_scan_prop *prop, void *extra) {
  struct htif_scan *scan = (struct htif_scan *)extra;
  if (!strcmp(prop->name, "compatible") && !strcmp((const char *)prop->value, "ucb,htif0")) {
    scan->compat = 1;
  }
}

static void htif_done(const struct fdt_scan_node *node, void *extra) {
  struct htif_scan *scan = (struct htif_scan *)extra;
  if (!scan->compat) return;

  htif = 1;
}

// scanning the HTIF
void query_htif(__uint64_t fdt) {
  struct fdt_cb cb;
  struct htif_scan scan;

  memset(&cb, 0, sizeof(cb));
  cb.open = htif_open;
  cb.prop = htif_prop;
  cb.done = htif_done;
  cb.extra = &scan;

  fdt_scan(fdt, &cb);
}

/////////////////////////    Spike HTIF basic s_operations    //////////////////////////
volatile uint64_t tohost __attribute__((section(".htif")));
volatile uint64_t fromhost __attribute__((section(".htif")));
//__htif_base marks the beginning of .htif section (defined in kernel/kernel.lds)
extern uint64_t __htif_base;

#define TOHOST(base_int) (uint64_t *)(base_int + TOHOST_OFFSET)
#define FROMHOST(base_int) (uint64_t *)(base_int + FROMHOST_OFFSET)

#define TOHOST_OFFSET ((__uint64_t)tohost - (__uint64_t)__htif_base)
#define FROMHOST_OFFSET ((__uint64_t)fromhost - (__uint64_t)__htif_base)

volatile int htif_console_buf;
static spinlock_t htif_lock = SPINLOCK_INIT;

static void __check_fromhost(void) {
  uint64_t fh = fromhost;
  if (!fh) return;
  fromhost = 0;

  // this should be from the console
  assert(FROMHOST_DEV(fh) == 1);
  switch (FROMHOST_CMD(fh)) {
    case 0:
      htif_console_buf = 1 + (uint8_t)FROMHOST_DATA(fh);
      break;
    case 1:
      break;
    default:
      assert(0);
  }
}

static void __set_tohost(__uint64_t dev, __uint64_t cmd, __uint64_t data) {
  while (tohost) __check_fromhost();
  tohost = TOHOST_CMD(dev, cmd, data);
}

static void do_tohost_fromhost(__uint64_t dev, __uint64_t cmd, __uint64_t data) {
  spinlock_lock(&htif_lock);
  __set_tohost(dev, cmd, data);

  while (1) {
    uint64_t fh = fromhost;
    if (fh) {
      if (FROMHOST_DEV(fh) == dev && FROMHOST_CMD(fh) == cmd) {
        fromhost = 0;
        break;
      }
      __check_fromhost();
    }
  }
  spinlock_unlock(&htif_lock);
}

/////////////////////    Encapsulated Spike HTIF functionalities    //////////////////
void htif_syscall(__uint64_t arg) { do_tohost_fromhost(0, 0, arg); }

// htif fuctionalities
void htif_console_putchar(uint8_t ch) {
#if __riscv_xlen == 32
  // HTIF devices are not supported on RV32, so proxy a write system call
  volatile uint64_t magic_mem[8];
  magic_mem[0] = HTIFSYS_write;
  magic_mem[1] = 1;
  magic_mem[2] = (__uint64_t)&ch;
  magic_mem[3] = 1;
  do_tohost_fromhost(0, 0, (__uint64_t)magic_mem);
#else
  spinlock_lock(&htif_lock);
  __set_tohost(1, 1, ch);
  spinlock_unlock(&htif_lock);
#endif
}

int htif_console_getchar(void) {
#if __riscv_xlen == 32
  // HTIF devices are not supported on RV32
  return -1;
#endif

  spinlock_lock(&htif_lock);
  __check_fromhost();
  int ch = htif_console_buf;
  if (ch >= 0) {
    htif_console_buf = -1;
    __set_tohost(1, 0, 0);
  }
  spinlock_unlock(&htif_lock);

  return ch - 1;
}

void htif_poweroff(void) {
  while (1) {
    fromhost = 0;
    tohost = 1;
  }
}
