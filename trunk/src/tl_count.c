
#include "tl_lock.h"
#include "tl_count.h"

#define TL_COUNT_MAX 4294967295
#define TL_COUNT_LEVEL	2

typedef tl_atomic_t tl_count_ele_t;

struct tl_count_t{
	
	tl_lock_t lock;
	tl_count_ele_t e[TL_COUNT_LEVEL];

};

void tl_count_init(tl_count_t *c, const char *v){

	tl_lock_init(&c->lock);
	tl_count_set(c, NULL == v ? tl_2string(0) : v);
}

static int tl_count_deserialize(tl_count_ele_t *ele, const char *v){
	return 0;
}

int tl_count_serialize(tl_count_ele_t *ele, const char *v, size_t n){
	return 0;
}

int tl_count_set(tl_count_t *c, const char *v){
	tl_count_ele_t ele[TL_COUNT_LEVEL];
	ele[0] = 0;
	return 0;
}

bool tl_count_cmp(int type, const tl_count_t *c1, const tl_count_t *c2){

	switch (type){
		case TL_C_EQ:
		case TL_C_LT:
			break;
		default:
			tl_assert(false);
			break;	
	}
	return false;
}

int tl_count_add(tl_count_t *c, const char *v){
	
	tl_count_ele_t ele[TL_COUNT_LEVEL];

	if(0 == tl_count_deserialize(ele, v)){

		int i;

		tl_lock_on(&c->lock);

		for(i = 0; i < TL_COUNT_LEVEL; i++){
			
			tl_count_ele_t tmp;
			
			tl_atomic_set(&tmp, tl_atomic_get(&c->e[i]));

			if(0 == tl_atomic_get(&ele[i])) { continue; }

			tl_atomic_fetch_add(&c->e[i], tl_atomic_get(&ele[i]));
			
			if(tl_atomic_get(&tmp) > tl_atomic_get(&c->e[i])){

				if(i < TL_COUNT_LEVEL - 1){
					tl_atomic_fetch_add(&c->e[i + 1], 1);
				}else{
					tl_error("tl_count overflow\n");
				}
			}
		}

		tl_lock_off(&c->lock);

		return 0;
	}else{
		return -1;
	}
}

int tl_count_sub(tl_count_t *c, const char *v){
	
	tl_count_ele_t ele[TL_COUNT_LEVEL];

	if(0 == tl_count_deserialize(ele, v)){

		int i;

		tl_lock_on(&c->lock);

		for(i = 0; i < TL_COUNT_LEVEL; i++){
			
			tl_count_ele_t tmp;
			
			tl_atomic_set(&tmp, tl_atomic_get(&c->e[i]));

			if(0 == tl_atomic_get(&ele[i])) { continue; }

			if(tl_atomic_get(&ele[i]) < tl_atomic_get(&c->e[i])){
				
				tl_atomic_set(&c->e[i], 0);

				if(i < TL_COUNT_LEVEL - 1){
					tl_atomic_fetch_dec(&c->e[i + 1]);
				}else{
					tl_error("[%d] overflow\n", __LINE__);
				}
			}else{
				tl_atomic_fetch_sub(&c->e[i], tl_atomic_get(&ele[i]));
			}
		}

		tl_lock_off(&c->lock);

		return 0;
	}else{
		return -1;
	}
}

int tl_count_inc(tl_count_t *c){
	return tl_count_add(c, tl_2string(1));
}

int tl_count_dec(tl_count_t *c){
	return tl_count_sub(c, tl_2string(1));
}
