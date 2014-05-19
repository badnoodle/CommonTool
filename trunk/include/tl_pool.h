#ifndef TOOL_POOL_H
#define TOOL_POOL_H

#include "tl_common.h"

#ifdef __cplusplus
extern "C"{
#endif
	
#define TL_POOL_DEBUG_ON 0

#if TL_POOL_DEBUG_ON
	#include "tl_profile.h"
#endif

	enum {
		TL_POOL_CNT_ALLOC,
		TL_POOL_CNT_INUSE,
		TL_POOL_CNT_BLK,
		TL_POOL_CNT_TOTALA,
		TL_POOL_CNT_FAIL_L/*arge*/,
		TL_POOL_CNT_FAIL_S_A/*bove*/,
		TL_POOL_CNT_FAIL_S_B/*elow*/,
		//TL_POOL_CNT_FAIL_S_F/*ull loop*/,
		TL_POOL_CNT_HIT_L/*arge*/,
		TL_POOL_CNT_HIT_S_I/*mmediately*/,
		TL_POOL_CNT_HIT_S_L/*oop*/,
		TL_POOL_CNT_HIT_S_S/*ystem*/,
		
		TL_POOL_CNT_F_ALIGN,

		TL_POOL_CNT_MAX,
		
		#if TL_POOL_DEBUG_ON
			TL_POOL_MAX_BKT = 14,
		#endif
	};

	struct tl_pool_t;
	typedef struct tl_pool_t tl_pool_t;

	tl_pool_t *tl_pool_init();

	void *tl_pool_alloc(tl_pool_t *, size_t);
	
	void *tl_pool_calloc(tl_pool_t *, size_t, size_t);
	
	void *tl_pool_realloc(tl_pool_t *, void *, size_t);
	
	void tl_pool_free(tl_pool_t *, void *);
	
	void tl_pool_destroy(tl_pool_t *);
	
	uintptr_t tl_pool_static(tl_pool_t *, size_t);

	#if TL_POOL_DEBUG_ON
	
	uintptr_t tl_pool_get_tag(tl_pool_t *, size_t);

	void tl_pool_traverse(tl_pool_t *tp);

	void tl_pool_check(tl_pool_t *, int, void *);

	tl_profile_t *tl_pool_getpro(tl_pool_t *);
	
	#endif

#ifdef __cplusplus
}
#endif

#endif
