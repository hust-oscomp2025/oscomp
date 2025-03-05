#ifndef _SPIKE_FILE_H_
#define _SPIKE_FILE_H_

#include <unistd.h>
#include <sys/stat.h>

#include <sys/types.h>

typedef struct file_t {
  int kfd;  // file descriptor of the host file
  __uint32_t refcnt;
} spike_file_t;

extern spike_file_t spike_files[];

#define MAX_FILES 128
#define MAX_FDS 128

#define ENOMEM 12 /* Out of memory */

#define stdin (spike_files + 0)
#define stdout (spike_files + 1)
#define stderr (spike_files + 2)

#define INIT_FILE_REF 3

struct frontend_stat {
  __uint64_t dev;
  __uint64_t ino;
  __uint32_t mode;
  __uint32_t nlink;
  __uint32_t uid;
  __uint32_t gid;
  __uint64_t rdev;
  __uint64_t __pad1;
  __uint64_t size;
  __uint32_t blksize;
  __uint32_t __pad2;
  __uint64_t blocks;
  __uint64_t atime;
  __uint64_t __pad3;
  __uint64_t mtime;
  __uint64_t __pad4;
  __uint64_t ctime;
  __uint64_t __pad5;
  __uint32_t __unused4;
  __uint32_t __unused5;
};

void copy_stat(struct stat* dest, struct frontend_stat* src);
spike_file_t* spike_file_open(const char* fn, int flags, int mode);
int spike_file_close(spike_file_t* f);
spike_file_t* spike_file_openat(int dirfd, const char* fn, int flags, int mode);
ssize_t spike_file_lseek(spike_file_t* f, size_t ptr, int dir);
ssize_t spike_file_read(spike_file_t* f, void* buf, size_t size);
ssize_t spike_file_pread(spike_file_t* f, void* buf, size_t n, off_t off);
ssize_t spike_file_write(spike_file_t* f, const void* buf, size_t n);
void spike_file_decref(spike_file_t* f);
void spike_file_init(void);
int spike_file_dup(spike_file_t* f);
int spike_file_truncate(spike_file_t* f, off_t len);
int spike_file_stat(spike_file_t* f, struct stat* s);
spike_file_t* spike_file_get(int fd);

#endif
