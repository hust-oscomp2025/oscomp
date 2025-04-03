// #include <kernel/vfs.h>
#include "forward_declarations.h"
#include <kernel/mm/vma.h>
#include <kernel/util.h>

int32 icache_init(void);
struct inode* icache_lookup(struct superblock* sb, uint64 ino);
uint32 icache_hash(const void* key);
void icache_insert(struct inode* inode);
void icache_delete(struct inode* inode);