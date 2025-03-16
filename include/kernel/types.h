#ifndef _TYPES_H_
#define _TYPES_H_
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>


/* 关于指针和数据类型的说明
 * uint64: 切实的数据值，不可能用作内存/内存运算
 *
 * 由于我们特殊的内核空间布局结构（物理内存总是会在内核页表中留一份直接映射）
 * 所以说paddr_t可以代表整个的内核物理地址空间+内核虚拟地址空间
 * paddr_t: 内核物理地址和内核虚拟地址（参与运算）
 * vaddr_t：用户虚拟地址（参与运算）
 * 
 * kptr_t: 内核空间指针
 * uptr_t: 用户空间指针
 * 
 * 
 * 所以说，内核的文档中的概念，需要严格区分指针和地址。
 */

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;

typedef signed char int8;
typedef signed short int16;
typedef signed int int32;
typedef signed long long int64;

#ifndef bool
typedef int bool;
#endif



// 方便的宏定义
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ROUNDUP(a, b) ((((a)-1) / (b) + 1) * (b))
#define ROUNDDOWN(a, b) ((a) / (b) * (b))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define __user
// depreciated 用来标记用户的虚拟内存地址
typedef uint64 uaddr;

// Physical/virtual addresses (64-bit on your architecture)
typedef uint64 paddr_t;     // Physical address
typedef uint64 vaddr_t;     // Virtual address

// User/kernel space pointers (for clarity in interfaces)
typedef void* kptr_t;          // Kernel pointer
typedef void* __user uptr_t;   // User space pointer

// fs相关
#define MOUNT_DEFAULT 0
#define MOUNT_AS_ROOT 1


#define MASK_FILEMODE 0x003

/* Timestamp structure */
struct timespec {
	int64 tv_sec;   /* Seconds */
	int64 tv_nsec;  /* Nanoseconds */
};



// file system type
/*
 * File type and permission bits
 */

#define MAX_FILE_NAME_LEN 256
typedef uint64 loff_t;
typedef uint32 mode_t;
typedef uint64_t sector_t;  /* 64-bit sector number */
/* File permissions and type mode */
typedef unsigned short mode_t;


struct dir {
  char name[MAX_FILE_NAME_LEN];
  int inum;
};

struct istat {
  int st_inum;
  int st_size;
  int st_type;
  int st_nlinks;
  int st_blocks;
};

#endif
