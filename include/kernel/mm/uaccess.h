#ifndef _UACCESS_H
#define _UACCESS_H
#include <kernel/types.h>

unsigned long copy_to_user(uint64 to, const uint64 from, unsigned long n);

#endif