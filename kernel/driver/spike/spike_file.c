/*
 * accessing host files by using the Spike interface.
 *
 * PKE OS needs to access the host file duing its execution to conduct ELF
 * (application) loading.
 *
 * codes are borrowed from riscv-pk (https://github.com/riscv/riscv-pk)
 */

#include <spike_interface/spike_file.h>
#include <util/atomic.h>
#include <util/spinlock.h>
#include <spike_interface/spike_htif.h>
#include <spike_interface/spike_utils.h>
#include <util/string.h>
#include <kernel/types.h>
// #include "../kernel/config.h"

#define atomic_cas(ptr, cmp, swp)                           \
  ({                                                        \
    long flags = disable_irqsave();                         \
    typeof(*(ptr)) res = *(volatile typeof(*(ptr))*)(ptr);  \
    if (res == (cmp)) *(volatile typeof(ptr))(ptr) = (swp); \
    enable_irqrestore(flags);                               \
    res;                                                    \
  })


static spike_file_t *spike_fds[MAX_FDS];
spike_file_t spike_files[MAX_FILES] = {[0 ... MAX_FILES - 1] = {-1, 0}};

void copy_stat(struct stat *dest_va, struct frontend_stat *src) {
  struct stat *dest = (struct stat *)dest_va;
  dest->st_dev = src->dev;
  dest->st_ino = src->ino;
  dest->st_mode = src->mode;
  dest->st_nlink = src->nlink;
  dest->st_uid = src->uid;
  dest->st_gid = src->gid;
  dest->st_rdev = src->rdev;
  dest->st_size = src->size;
  dest->st_blksize = src->blksize;
  dest->st_blocks = src->blocks;
  dest->st_atime = src->atime;
  dest->st_mtime = src->mtime;
  dest->st_ctime = src->ctime;
}

int spike_file_stat(spike_file_t *f, struct stat *s) {
  struct frontend_stat buf;
  __uint64_t pa = (__uint64_t)&buf;
  long ret =
      frontend_syscall(HTIFSYS_fstat, f->kfd, (__uint64_t)&buf, 0, 0, 0, 0, 0);
  copy_stat(s, &buf);
  return ret;
}

int spike_file_close(spike_file_t *f) {
  if (!f)
    return -1;
  spike_file_t *old = atomic_cas(&spike_fds[f->kfd], f, 0);
  spike_file_decref(f);
  if (old != f)
    return -1;
  spike_file_decref(f);
  return 0;
}

void spike_file_decref(spike_file_t *f) {
  if (atomic_fetch_add(&f->refcnt, -1) == 2) {
    int kfd = f->kfd;
    atomic_thread_fence(memory_order_seq_cst);
    __atomic_store_n(&f->refcnt, 0, __ATOMIC_SEQ_CST);

    frontend_syscall(HTIFSYS_close, kfd, 0, 0, 0, 0, 0, 0);
  }
}

void spike_file_incref(spike_file_t *f) {
  long prev = atomic_fetch_add(&f->refcnt, 1);
  kassert(prev > 0);
}

ssize_t spike_file_write(spike_file_t *f, const void *buf, size_t size) {
  return frontend_syscall(HTIFSYS_write, f->kfd, (__uint64_t)buf, size, 0, 0, 0, 0);
}

static spike_file_t *spike_file_get_free(void) {
  for (spike_file_t *f = spike_files; f < spike_files + MAX_FILES; f++)
    if (__atomic_load_n(&f->refcnt, __ATOMIC_SEQ_CST) == 0 &&
        atomic_cas(&f->refcnt, 0, INIT_FILE_REF) == 0)
      return f;
  return NULL;
}

int spike_file_dup(spike_file_t *f) {
  for (int i = 0; i < MAX_FDS; i++) {
    if (atomic_cas(&spike_fds[i], 0, f) == 0) {
      spike_file_incref(f);
      return i;
    }
  }
  return -1;
}

void spike_file_init(void) {
  // create stdin, stdout, stderr and FDs 0-2
  for (int i = 0; i < 3; i++) {
    spike_file_t *f = spike_file_get_free();
    f->kfd = i;
    spike_file_dup(f);
  }
}

spike_file_t *spike_file_openat(int dirfd, const char *fn, int flags,
                                int mode) {
  spike_file_t *f = spike_file_get_free();
  if (f == NULL)
    return ERR_PTR(-ENOMEM);

  size_t fn_size = strlen(fn) + 1;
  long ret = frontend_syscall(HTIFSYS_openat, dirfd, (__uint64_t)fn, fn_size, flags,
                              mode, 0, 0);
  if (ret >= 0) {
    f->kfd = ret;
    return f;
  } else {
    spike_file_decref(f);
    return ERR_PTR(ret);
  }
}

spike_file_t *spike_file_open(const char *fn, int flags, int mode) {
  return spike_file_openat(-100, fn, flags, mode);
}

ssize_t spike_file_pread(spike_file_t *f, void *buf, size_t size,
                         off_t offset) {
  return frontend_syscall(HTIFSYS_pread, f->kfd, (__uint64_t)buf, size, offset, 0,
                          0, 0);
}

ssize_t spike_file_read(spike_file_t *f, void *buf, size_t size) {
  return frontend_syscall(HTIFSYS_read, f->kfd, (__uint64_t)buf, size, 0, 0, 0, 0);
}

ssize_t spike_file_lseek(spike_file_t *f, size_t ptr, int dir) {
  return frontend_syscall(HTIFSYS_lseek, f->kfd, ptr, dir, 0, 0, 0, 0);
}

spike_file_t *spike_file_get(int fd) {
  spike_file_t *f;
  if (fd < 0 || fd >= MAX_FDS ||
      (f = __atomic_load_n(&spike_fds[fd], __ATOMIC_SEQ_CST)) == NULL)
    return 0;

  long old_cnt;
  do {
    old_cnt = __atomic_load_n(&f->refcnt, __ATOMIC_SEQ_CST);
    if (old_cnt == 0)
      return 0;
  } while (atomic_cas(&f->refcnt, old_cnt, old_cnt + 1) != old_cnt);

  return f;
}
