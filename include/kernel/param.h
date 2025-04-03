// from farmos
#ifndef _PARAM_H
#define _PARAM_H
#include <kernel/feature.h>
#define NCPU 1
#ifndef NCPU
#error NCPU not defined
#endif	       // !NCPU

#define MAXARG 256		  // max exec arguments
#define MAXARGLEN 256		  // max exec argument length
#define MAXPATH 128		  // maximum file path name
#define MAX_PROC_NAME_LEN (MAXPATH + 1)

// FarmOS 参数
#define NTHREAD NPROC       // FarmOS 支持的最大线程数
#define NPROCSIGNALS 128     // FarmOS 支持的最大信号数
#define NSIGEVENTS 512      // FarmOS 支持的最大信号事件数

#endif
