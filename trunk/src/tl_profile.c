
#include "tl_profile.h"
#include "tl_assert.h"
#include "tl_trace.h"

struct tl_profile_t{
	size_t maxL;
	size_t curL;
	size_t totalL;

	tl_rbtree_root_t *root;
	tl_profile_entry_t *first;
	tl_profile_entry_t *lastI/*nterrupt*/;
	tl_profile_entry_t *lastE/*nd*/;
};

static int keycmp(const void* p1, const void* p2){
	return strcmp((const char *)p1, (const char *)p2);
}

static tl_profile_entry_t *newEntry(tl_profile_t *p, const char *func){
	tl_keyval_t kv = {(tl_key_t)func, NULL};
	tl_profile_item_t *item = NULL;
	tl_profile_entry_t *pnew = (tl_profile_entry_t *)calloc(1, sizeof(tl_profile_entry_t));

	if(NULL == pnew) { goto L_failed; }

	if(-1 == tl_rbtree_query(p->root, &kv)){
		item = (tl_profile_item_t *)calloc(1, sizeof(tl_profile_item_t));
		item->func = strdup(func);
		if(NULL == item->func) {
			goto L_failed;
		}

		kv.key = (tl_key_t)item->func;
		kv.val = (tl_val_t)item;

		if(-1 == tl_rbtree_insert(p->root, &kv, false)){
			goto L_failed;
		}
	}

	pnew->item = (tl_profile_item_t *)kv.val;

	return pnew;

L_failed:
	if(NULL != pnew){
		if(NULL != pnew->item){
			if(NULL != pnew->item->func){
				free((void *)pnew->item->func);
			}
			free(pnew->item);
		}
		free(pnew);
	}
	return NULL;
}

static inline void freeItem(tl_profile_t *p, tl_profile_item_t *item){
	if(NULL !=p) {}
	free((void *)item->func);
	free(item);
}

static inline void freeEntry(tl_profile_t *p, tl_profile_entry_t *e){
	if(NULL !=p) {}
	free(e);
}

tl_profile_t *tl_profile_init(size_t maxLevel){
	tl_profile_t *p = (tl_profile_t *)calloc(1, sizeof(tl_profile_t));
	if(NULL == p) { return NULL; }
	p->root = tl_rbtree_init(&(rb_config_t){keycmp});
	if(NULL == p->root) { free(p); p = NULL; }
	p->maxL = maxLevel;
	return p;
}

void tl_profile_destroy(tl_profile_t *p){
	/*
	 *	FIXME
	 * */
	tl_profile_entry_t *pcur = p->first;
	tl_profile_entry_t *plast = pcur;;
	while(NULL != plast){
		pcur = plast->next;
		freeEntry(p, plast);
		plast = pcur;
	}

	tl_rbtree_entry_t *entry = alloca(tl_rbtree_entrySize());
	if(-1 != tl_rbtree_first(p->root, entry)){
		tl_keyval_t kv;
		do{
			tl_rbtree_extract(entry, &kv);
			freeItem(p, (tl_profile_item_t *)kv.val);
		}while(-1 != tl_rbtree_next(entry));
	}

	tl_rbtree_destroy(p->root);

	free(p);
}

void tl_profile_start(tl_profile_t *p, const char *func){
	p->totalL ++;
	p->curL ++;

	if(0 < p->maxL && p->curL > p->maxL) { return; }

	tl_profile_entry_t *pnew = newEntry(p, func);
	if(NULL == pnew) {
		return;
	}

	if(NULL == p->first) { p->first = pnew; }

	pnew->next = p->lastI;
	p->lastI = pnew;
	pnew->item->cnt_call ++;

	pnew->level = p->curL;
	pnew->ut.cpu = clock();
	if(-1 == gettimeofday(&(pnew->ut.real), NULL)){
		/*
		 *	FIXME
		 * */
		tl_assert(false);
	}

	return;
}

void tl_profile_end(tl_profile_t *p, const char *func){

	struct timeval rt;
	clock_t ce = clock();
	tl_profile_entry_t *end = NULL;

	if(NULL != func) {}

	if(-1 == gettimeofday(&rt, NULL)){
		/*
		 *	FIXME
		 * */
		tl_assert(false, tl_trace("[in %s:%d]\n", __FUNCTION__, __LINE__));
	}

	if(NULL == p->lastI) { 
		return; 
	}else if(p->curL != p->lastI->level){
		if(p->curL > p->lastI->level){

			tl_error("forget to call <profile exit> for [%s]\n", p->lastI->item->func);

			tl_profile_entry_t *tmp = p->lastI;
			p->lastI = p->lastI->next;
			freeEntry(p, tmp);
		}
		return;
	}

	p->curL --;

	/*
	 *	calculate time part ...
	 * */

	end = p->lastI;

	end->ut.real.tv_sec = rt.tv_sec - end->ut.real.tv_sec;

	if(rt.tv_usec > end->ut.real.tv_usec){
		end->ut.real.tv_usec = rt.tv_usec - end->ut.real.tv_usec;
	}else{
		end->ut.real.tv_sec -= 1;
		end->ut.real.tv_usec = 1000000 + rt.tv_usec - end->ut.real.tv_usec;
	}

	end->ut.cpu = ce - end->ut.cpu;

	/*
	 *	FIXME
	 *		HOW DEAL OVERFLOW
	 * */
	end->item->total.cpu += end->ut.cpu;
	end->item->total.real.tv_sec += end->ut.real.tv_sec;
	end->item->total.real.tv_usec += end->ut.real.tv_usec;
	if(1000000 <= end->item->total.real.tv_usec){
		end->item->total.real.tv_usec -= 1000000;
		end->item->total.real.tv_sec ++;
	}

	p->lastI = end->next;

	end->next = NULL;

	if(NULL != p->lastE){
		p->lastE->next = end;
	}else{
		p->first = end;
	}
	p->lastE = end;
}

const tl_profile_entry_t *tl_profile_getEntry(const tl_profile_t *p){
	return p->first;
}

int tl_profile_first(const tl_profile_t *p, tl_rbtree_entry_t *entry){
	return tl_rbtree_first(p->root, entry);
}

int tl_profile_next(tl_rbtree_entry_t *entry){
	return tl_rbtree_next(entry);
}

const tl_profile_item_t *tl_profile_getItem(const tl_profile_t *p, const char *func){
	tl_keyval_t kv = {(tl_key_t)func, NULL};
	if(0 == tl_rbtree_query(p->root, &kv)){
		return (const tl_profile_item_t *)kv.val;
	}
	return NULL;
}
