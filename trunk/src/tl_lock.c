
#include "tl_common.h"
#include "tl_lock.h"

void tl_lock_init(tl_lock_t *l){
	tl_atomic_set(l, 0);
}

void tl_lock_on(tl_lock_t *l){

	tl_atomic_t o;

	while(true){

		tl_atomic_set(&o, tl_atomic_get(l));

		if(0 == (0X80000000 & o) && tl_atomic_cmp_set(l, o, 0X80000000 | o)){
			return;
		}

		tl_yield();
	}
}

void tl_lock_off(tl_lock_t *l){

	uint32_t o, w, v;

	while(true){

		tl_atomic_set(&o, tl_atomic_get(l));

		w = 0X7FFFFFFF & o;

		v = 0 == w ? 0X7FFFFFFF : w - 1;

		if(tl_atomic_cmp_set(l, o, v)){
			break;
		}
	}
}

int tl_lock_tryon(tl_lock_t *l){

	tl_atomic_t o;

	tl_atomic_set(&o, tl_atomic_get(l));

	if(0 == (0X80000000 & o) && tl_atomic_cmp_set(l, o, 0X80000000 | o)){
		return 0;
	}

	return -1;
}

