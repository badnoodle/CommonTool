#ifndef TOOL_HEAP_H
#define TOOL_HEAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tl_config.h"
#include "tl_common.h"
#include "tl_pool.h"

struct tl_heap_t;
typedef struct tl_heap_t tl_heap_t;

typedef void (*getIndexHandle)(void *, size_t, void *);

tl_heap_t *tl_heap_init(tl_pool_t *, size_t ele_size, size_t ele_cnt, keycmpHandle h, bool raft);

void tl_heap_destroy(tl_heap_t *);

size_t tl_heap_size(tl_heap_t *);

int tl_heap_add(tl_heap_t *, const void *);
void tl_heap_add_and_replace(tl_heap_t *, const void *);

const void *tl_heap_top(tl_heap_t *);

int tl_heap_pop(tl_heap_t *, void * /*NULL*/);

void tl_heap_adjust(void *cont, size_t ele_size, keycmpHandle kc, bool raft, size_t b, size_t e);

#if 0
void tl_heap_down(void *, size_t, keycmpHandle kc, getIndexHandle, bool raft, size_t , size_t, void *);
void tl_heap_up(void *, size_t, keycmpHandle kc, getIndexHandle, bool raft, size_t, void *);
#else
void tl_heap_down(void *, size_t, size_t, keycmpHandle kc, getIndexHandle, bool raft, size_t , void *);
void tl_heap_up(void *, size_t, size_t, keycmpHandle kc, getIndexHandle, bool raft, size_t, void *);
#endif

#ifdef __cplusplus
}
#endif

#endif

