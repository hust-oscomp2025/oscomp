#pragma once
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mount.h>
#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#include <poll.h>

// #include <bits/fcntl.h>
// #include <sys/cdefs.h>
// #include <kernel/riscv.h>

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

// 方便的宏定义
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ROUNDUP(a, b) ((((a) - 1) / (b) + 1) * (b))
#define ROUNDDOWN(a, b) ((a) / (b) * (b))

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define __user
// depreciated 用来标记用户的虚拟内存地址
typedef uint64 uaddr;

// Physical/virtual addresses (64-bit on your architecture)
typedef uint64 paddr_t;  // Physical address
typedef uint64 vaddr_t;  // Virtual address
typedef uint64 time64_t; // 64-bit time value
typedef unsigned int fmode_t;

// User/kernel space pointers (for clarity in interfaces)
typedef void* kptr_t;        // Kernel pointer
typedef void* __user uptr_t; // User space pointer

// fs相关
#define MOUNT_DEFAULT 0
#define MOUNT_AS_ROOT 1

#define MASK_FILEMODE 0x003

/* Basic time types and structures */

// timespec64 structure if not already defined
struct timespec64 {
	time64_t tv_sec; /* seconds */
	long tv_nsec;    /* nanoseconds */
};

/*
 * POSIX-compatible time zone structure
 */
struct timezone {
	int tz_minuteswest; /* minutes west of Greenwich */
	int tz_dsttime;     /* type of DST correction */
};

/*
 * Timer interval specification
 */
struct itimerspec {
	struct timespec it_interval; /* timer interval */
	struct timespec it_value;    /* initial expiration */
};

/*ERRNO TYPES*/
#define MAX_ERRNO 4095L
#define IS_ERR_VALUE(x) ((unsigned long)(x) >= (unsigned long)-MAX_ERRNO)
#define unlikely_if(x) if (unlikely(x))
#define ERR_PTR(err) ((void*)((long)(err)))
#define PTR_ERR(ptr) ((long)(ptr))
#define PTR_IS_ERROR(ptr) IS_ERR_VALUE((unsigned long)(ptr))
// clang-format off
#define CHECK_PTR_VALID(ptr, errno_val) \
    do { if (unlikely(!(ptr) || PTR_IS_ERROR(ptr))) return (errno_val); } while (0)
#define CHECK_PTR_ERROR(ptr, errno_val) \
	do { if (unlikely(PTR_IS_ERROR(ptr))) return (errno_val); } while (0)

#define PTR_IS_INVALID(ptr) (!(ptr) || PTR_IS_ERROR(ptr))

#define CHECK_PTRS(errno_val, ...)                         \
		do {                                                                \
			void* __ptrs[] = { (void*)__VA_ARGS__ };                        \
			for (size_t __i = 0; __i < sizeof(__ptrs)/sizeof(void*); ++__i) { \
				if (PTR_IS_INVALID(__ptrs[__i]))                                \
					return (errno_val);                                     \
			}                                                               \
		} while (0)
#define CHECK_RET(ret, errno_val) \
	do { if (unlikely((ret) < 0)) return (errno_val); } while (0)
// clang-format on

// vma
/* Virtual memory fault return type */
typedef unsigned int vm_fault_t;
/* VM fault status codes */
#define VM_FAULT_OOM 0x000001
#define VM_FAULT_SIGBUS 0x000002
#define VM_FAULT_MAJOR 0x000004
#define VM_FAULT_WRITE 0x000008 /* Write access, not read */
#define VM_FAULT_HWPOISON 0x000010
#define VM_FAULT_RETRY 0x000020
#define VM_FAULT_NOPAGE 0x000040 /* No page was found */
#define VM_FAULT_LOCKED 0x000080
#define VM_FAULT_DONE_COW 0x000100
#define VM_FAULT_NEEDDSYNC 0x000200

// file system type
/*
 * File type and permission bits
 */

#define MAX_FILE_NAME_LEN 256

typedef uint64_t sector_t; /* 64-bit sector number */
/* File permissions and type mode */
typedef unsigned int __poll_t;

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

typedef long __fsword_t;
typedef unsigned long fsblkcnt_t;

/* File permission and type bits */
typedef unsigned short umode_t;
#define READ 0
#define WRITE 1

/**
 * 测试位是否被置位（返回1或0）
 * @param nr 位编号（从0开始）
 * @param addr 位图指针
 */
static inline int test_bit(int nr, const volatile uint64* addr) { return (addr[nr / (8 * sizeof(uint64))] >> (nr % (8 * sizeof(uint64)))) & 1UL; }

/**
 * 设置指定位置的位
 * @param nr 位编号（从0开始）
 * @param addr 位图指针
 */
static inline void set_bit(int nr, volatile uint64* addr) { addr[nr / (8 * sizeof(uint64))] |= (1UL << (nr % (8 * sizeof(uint64)))); }

/**
 * 清除指定位置的位
 * @param nr 位编号（从0开始）
 * @param addr 位图指针
 */
static inline void clear_bit(int nr, volatile uint64* addr) { addr[nr / (8 * sizeof(uint64))] &= ~(1UL << (nr % (8 * sizeof(uint64)))); }

/*device types*/
/* Major/minor number manipulation macros */
#define MINORBITS 20
#define MINORMASK ((1U << MINORBITS) - 1)

/* Extract major and minor numbers from a device ID */
#define MAJOR(dev) ((unsigned int)((dev) >> MINORBITS))
#define MINOR(dev) ((unsigned int)((dev) & MINORMASK))

/* Create a device ID from major and minor numbers */
#define MKDEV(major, minor) (((dev_t)(major) << MINORBITS) | (minor))

/* Standard Linux device major numbers */
#define UNNAMED_MAJOR 0
#define RAMDISK_MAJOR 1
#define FLOPPY_MAJOR 2
#define IDE0_MAJOR 3
#define IDE1_MAJOR 22
#define IDE2_MAJOR 33
#define IDE3_MAJOR 34
#define SCSI_DISK0_MAJOR 8
#define SCSI_DISK1_MAJOR 65
#define SCSI_DISK2_MAJOR 66
#define SCSI_DISK3_MAJOR 67
#define SCSI_DISK4_MAJOR 68
#define SCSI_DISK5_MAJOR 69
#define SCSI_DISK6_MAJOR 70
#define SCSI_DISK7_MAJOR 71
#define LOOP_MAJOR 7
#define MMC_BLOCK_MAJOR 179
#define VIRTBLK_MAJOR 254

/* Special purpose device majors */
#define MEM_MAJOR 1 /* /dev/mem etc */
#define TTY_MAJOR 4
#define TTYAUX_MAJOR 5
#define RANDOM_MAJOR 1 /* /dev/random /dev/urandom */

#define DYNAMIC_MAJOR_MIN 128 /* Reserve first 128 majors for fixed assignments */

typedef unsigned int vm_fault_t;

/* Encode hstate index for a hwpoisoned large page */
#define VM_FAULT_SET_HINDEX(x) ((vm_fault_t)((x) << 16))
#define VM_FAULT_GET_HINDEX(x) (((uint32)(x) >> 16) & 0xf)

#define VM_FAULT_ERROR (VM_FAULT_OOM | VM_FAULT_SIGBUS | VM_FAULT_SIGSEGV | VM_FAULT_HWPOISON | VM_FAULT_HWPOISON_LARGE | VM_FAULT_FALLBACK)



#include <sys/stat.h>

// struct stat {
// 	dev_t st_dev;
// 	ino_t st_ino;
// 	mode_t st_mode;
// 	nlink_t st_nlink;
// 	uid_t st_uid;
// 	gid_t st_gid;
// 	dev_t st_rdev;
// 	unsigned long long __pad;
// 	off_t st_size;
// 	blksize_t st_blksize;
// 	int __pad2;
// 	blkcnt_t st_blocks;
// 	struct timespec st_atim;
// 	struct timespec st_mtim;
// 	struct timespec st_ctim;
// 	unsigned __unused[2];

// };




