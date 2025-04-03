#include <kernel/types.h>
#include <kernel/mmu.h>
#include <kernel/util.h> 

int ffs(int x)
{
    int r = 1;
    
    if (x == 0)
        return 0;
        
    if ((x & 0xffff) == 0) {
        x >>= 16;
        r += 16;
    }
    
    if ((x & 0xff) == 0) {
        x >>= 8;
        r += 8;
    }
    
    if ((x & 0xf) == 0) {
        x >>= 4;
        r += 4;
    }
    
    if ((x & 0x3) == 0) {
        x >>= 2;
        r += 2;
    }
    
    if ((x & 0x1) == 0) {
        x >>= 1;
        r += 1;
    }
    
    return r;
}


void qsort(void *base, size_t nmemb, size_t size, __compar_fn_t compar) {
    // 简单的冒泡排序实现
    char *baseptr = (char *)base;
    char *temp = (char *)kmalloc(size);  // 需要先实现一个简单的malloc
    
    if (!temp) return;  // 内存分配失败
    
    for (size_t i = 0; i < nmemb - 1; i++) {
        for (size_t j = 0; j < nmemb - i - 1; j++) {
            char *elem1 = baseptr + j * size;
            char *elem2 = baseptr + (j + 1) * size;
            
            if (compar(elem1, elem2) > 0) {
                // 交换元素
                memcpy(temp, elem1, size);
                memcpy(elem1, elem2, size);
                memcpy(elem2, temp, size);
            }
        }
    }
    
    kfree(temp);  // 需要先实现一个简单的free
}