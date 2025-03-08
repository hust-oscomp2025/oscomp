#include <kernel/vfs.h>
#include <kernel/proc_file.h>

#include <kernel/process.h>
#include <spike_interface/spike_utils.h>

// int sys_fstat(uint64 fd, uint64 stat)
// {

// 	return do_stat(fd,user_va_to_pa(CURRENT->pagetable,(void*)stat));
// }