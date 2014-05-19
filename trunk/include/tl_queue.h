
#ifndef TOOL_QUEUE_H
#define TOOL_QUEUE_H

#ifdef __cplusplus
extern "C"{
#endif

    #include "tl_common.h"

    typedef const void *tl_queue_ele_t;

    struct tl_queue_t;
    typedef struct tl_queue_t tl_queue_t;

    tl_queue_t *tl_queue_init(size_t);
    
    int tl_queue_push(tl_queue_t *, tl_queue_ele_t);
    
    void tl_queue_pop(tl_queue_t *);
    
    size_t tl_queue_size(tl_queue_t *);
    
    tl_queue_ele_t tl_queue_top(tl_queue_t *);
    
    void tl_queue_destroy(tl_queue_t *);

#ifdef __cplusplus
}
#endif

#endif
