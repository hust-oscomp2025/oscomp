#ifndef _TYPES_H_
#define _TYPES_H_
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

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
#define ROUNDUP(a, b) ((((a)-1) / (b) + 1) * (b))
#define ROUNDDOWN(a, b) ((a) / (b) * (b))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))


// 用来标记用户的虚拟内存地址
#define __user

#define MOUNT_DEFAULT 0
#define MOUNT_AS_ROOT 1


#define MASK_FILEMODE 0x003



// file system type


#define MAX_FILE_NAME_LEN 256
typedef uint64 loff_t;

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
