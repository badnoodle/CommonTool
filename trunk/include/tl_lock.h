#ifndef TOOL_LOCK_H
#define TOOL_LOCK_H

#include "tl_atomic.h"

#ifdef __cplusplus
extern "C" {
#endif

	typedef tl_atomic_t tl_lock_t;

	void tl_lock_init(tl_lock_t *);

	void tl_lock_on(tl_lock_t *);

	void tl_lock_off(tl_lock_t *);

	int tl_lock_tryon(tl_lock_t *);

#ifdef __cplusplus
}
#endif

#endif

