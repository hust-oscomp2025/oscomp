#ifndef _SYSCALL_FILE_H_
#define _SYSCALL_FILE_H_
#include <kernel/types.h>

int64 sys_mount(const char* source_user, const char* target_user, const char* fstype_user, uint64 flags, const void* data_user);


#endif