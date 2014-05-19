
#include "tl_common.h"

uintptr_t tl_pagesize = 4096;

#ifdef FREEBSD

void *tl_memalign(size_t align, size_t size){
    void *ptr;
    if(0 != posix_memalign(&ptr, align, size)){
        return NULL;
    }
    return ptr;
}

#endif

