#pragma once
#include <kernel/types.h>

int32 create_device_nodes(void);
void device_init(void);
int32 lookup_dev_id(const char* dev_name, dev_t* dev_id);