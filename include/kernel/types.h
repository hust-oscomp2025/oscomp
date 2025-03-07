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

#define __user

   

#define MOUNT_DEFAULT 0
#define MOUNT_AS_ROOT 1


#define MASK_FILEMODE 0x003

#define FD_NONE 0
#define FD_OPENED 1

#define MAX_FILE_NAME_LEN 32

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
