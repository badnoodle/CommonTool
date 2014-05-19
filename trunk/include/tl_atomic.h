#ifndef TOOL_ATOMIC_H
#define TOOL_ATOMIC_H

#include <stdint.h>

#if defined(__GNUC__) && ( __GNUC__ * 100 + __GNUC_MINOR__ >= 401)

    typedef volatile uint32_t tl_atomic_t;
	
    #define tl_atomic_get(p)      ( *p )

    #define tl_atomic_set(p, val)   __sync_lock_test_and_set((tl_atomic_t *)(p), val)

    #define tl_atomic_fetch_inc(p)	__sync_fetch_and_add(( tl_atomic_t *)(p), 1)
    
	#define tl_atomic_fetch_dec(p)	__sync_fetch_and_sub(( tl_atomic_t *)(p), 1)
    
	#define tl_atomic_fetch_add(p, inc)	__sync_fetch_and_add(( tl_atomic_t *)(p), (inc))

    #define tl_atomic_fetch_sub(p, dec)	__sync_fetch_and_sub(( tl_atomic_t *)(p), (dec))

	#define tl_atomic_cmp_set(p, o, s)	__sync_bool_compare_and_swap(( tl_atomic_t *)p, o, s)

#else

#error "no atomic support"

#endif

#endif

