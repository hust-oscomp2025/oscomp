#include <kernel/types.h>

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