
#include "tl_config.h"
#include "tl_pool.h"
#include "tl_atomic.h"

#if (TL_SUPPORT_MMAP)
#include <sys/mman.h>
#endif

#if (TL_SUPPORT_MULTI)
#include <pthread.h>
#endif

#define TL_ALIGNMENT	sizeof(unsigned long)
#define TL_POOL_BASE_INDEX	0
#define TL_POOL_MAX_INDEX	24
#define TL_POOL_THRESH_INDEX	9

#define tl_align(a, p)			((((uintptr_t)p + (uintptr_t)a - 1)) & ~((uintptr_t)a - 1))

#define tl_pool_tag_foot(tag)	((tl_pool_tag_t *)((uintptr_t)(tag) + (tag)->size - tl_pool_tag_tailS))

#define tl_pool_tag_headS		(sizeof(tl_pool_tag_t))
#define tl_pool_tag_tailS		(sizeof(uintptr_t) + sizeof(uintptr_t))

#define tl_pool_tag_unuseS		(tl_pool_tag_headS + tl_pool_tag_tailS - sizeof(uintptr_t))

#define tl_pool_tag_thresh		(tl_pool_tag_headS + tl_pool_tag_tailS)

#if TL_POOL_DEBUG_ON

#define TP_PROFILE_ENTER(name)	\
	tl_profile_start(tp->pro, (name));

#define TP_PROFILE_EXIT(name)	\
	tl_profile_end(tp->pro, (name));

#else

#define TP_PROFILE_ENTER(n)
#define	TP_PROFILE_EXIT(n)

#endif

#if TL_SUPPORT_MULTI

#define tl_pool_lock(tp)	pthread_mutex_lock(&(tp)->mutex)
#define	tl_pool_unlock(tp)	pthread_mutex_unlock(&(tp)->mutex)

#else

#define tl_pool_lock(tp)
#define tl_pool_unlock(tp)

#endif

struct tl_pool_blk_t;

typedef struct tl_pool_tag_t{

	union {
		struct {
			union {
				struct tl_pool_blk_t *t_blk;
				uintptr_t	use;
			};

			union{
				struct tl_pool_tag_t *pre;
				struct tl_pool_tag_t *ref;
			};

			uintptr_t size;

			union {
				struct tl_pool_tag_t *suc;
				struct tl_pool_blk_t *h_blk;
			};
		};

		uintptr_t a[4];
	};

}tl_pool_tag_t;

typedef struct tl_pool_blk_t{
	struct tl_pool_blk_t *pre;
	struct tl_pool_blk_t *nxt;
	size_t size;
}tl_pool_blk_t;

struct tl_pool_t{

	union {

		struct {

			uintptr_t cnt_alloc;
			uintptr_t cnt_inuse;
			uintptr_t cnt_blk;
			uintptr_t cnt_totalA;

			uintptr_t cnt_l_fail;
			uintptr_t cnt_s_fail_1;	/* fail since above thresh */
			uintptr_t cnt_s_fail_2;	/* fail since blow thresh */
			//uintptr_t cnt_s_fail_3;	/* fail with full loop */

			uintptr_t cnt_l_hit;	/* hit from sys call for large blk*/
			uintptr_t cnt_s_hit_1;	/* hit immediately */
			uintptr_t cnt_s_hit_2;	/* hit from the max tag in bkt with loop */
			uintptr_t cnt_s_hit_3;	/* hit from sys call for normal blk */

			uintptr_t cnt_f_align;	/* f addr align */

		};
		uintptr_t cnt[12];

	};

#if TL_SUPPORT_MULTI
	pthread_mutex_t mutex;
	pthread_mutex_attr_t attr;
#endif

#if TL_POOL_DEBUG_ON
	tl_profile_t *pro;
	uintptr_t cnt_tag[TL_POOL_MAX_INDEX];
#endif

	tl_pool_tag_t *bkt[TL_POOL_MAX_INDEX];

	/*
	 *	any field should locate before blk
	 * */
	tl_pool_blk_t blk;
};

tl_pool_t *afx_pool = NULL;

__attribute__((constructor)) static void before_main();

static void before_main(){
	afx_pool = tl_pool_init();
}

static inline int tl_pool_size2idx(size_t s){

	int r;

	asm (
			"bsr %0,%0\n" : "=&q" (r) : "0" (s)
		);

	if(s > (size_t)(1 << r)){
		r ++;
	}

	if(TL_POOL_BASE_INDEX >= r){
		return 0;
	}

	r -=  TL_POOL_BASE_INDEX;

	if(TL_POOL_MAX_INDEX <= r){
		return TL_POOL_MAX_INDEX;
	}
	return r;
}

#if TL_POOL_DEBUG_ON

static inline void tl_pool_tag_info(tl_pool_tag_t *tag){
	if(0 == tag->use){
		tl_info("[H][%X:%d:%d] [T][%X]", (uintptr_t)tag, tag->use, tag->size, (uintptr_t)tl_pool_tag_foot(tag)->t_blk);
	}else{
		tl_info("[H][%X:%d:%d:%X]", (uintptr_t)tag, tag->use, tag->size, (uintptr_t)tag->h_blk);
	}
}

#endif

static inline void tl_pool_add_tag(tl_pool_t *tp, int idx, tl_pool_tag_t *tag){

	tl_pool_lock(tp);

#if TL_POOL_DEBUG_ON
	tp->cnt_tag[idx] ++;
#endif

	if(NULL == tp->bkt[idx]){
		tp->bkt[idx] = tag;
		tag->pre = tag;
		tag->suc = tag;
	}else{
		tag->suc = tp->bkt[idx];
		tag->pre = tp->bkt[idx]->pre;
		tp->bkt[idx]->pre->suc = tag;
		tp->bkt[idx]->pre = tag;
		tp->bkt[idx] = tag;
	}

	tl_pool_unlock(tp);
}

static inline void tl_pool_rm_tag(tl_pool_t *tp, tl_pool_tag_t *tag){

	int idx = tl_pool_size2idx(tag->size);

	tl_pool_lock(tp);

#if TL_POOL_DEBUG_ON
	tp->cnt_tag[idx] --;
#endif

	if(tag == tag->suc){
		tp->bkt[idx] = NULL;
	}else{
		if(tag == tp->bkt[idx]){
			tp->bkt[idx] = tag->suc;
		}
		tag->pre->suc = tag->suc;
		tag->suc->pre = tag->pre;
	}

	tl_pool_unlock(tp);
}

static inline tl_pool_tag_t *tl_pool_tag_first(tl_pool_blk_t *blk){
	return (tl_pool_tag_t *)((uintptr_t)blk + sizeof(tl_pool_blk_t));
}

static inline tl_pool_tag_t *tl_pool_tag_nxt(tl_pool_blk_t *blk, tl_pool_tag_t *tag){
	if(((uintptr_t)blk + blk->size) == ((uintptr_t)tag + tag->size)){
		return NULL;
	}else{
		return (tl_pool_tag_t *)((uintptr_t)tag + tag->size);
	}
}

static inline void *tl_pool_tag_alloc(tl_pool_t *tp, tl_pool_tag_t *tag, size_t size){

	tl_pool_tag_t *f = tl_pool_tag_foot(tag);
	tl_pool_tag_t *nt   = NULL;
	uintptr_t addr;

	if(size > tag->size){
		return NULL;
	}

	addr = (uintptr_t)(tag) + tl_pool_tag_headS;

	tl_assert(f->ref == tag);

	tl_pool_rm_tag(tp, tag);

	if(tl_pool_tag_thresh >= (tag->size - size)){

		//TP_PROFILE_ENTER("tl pool stage 6");

		tag->use = 1;

		tag->h_blk = f->t_blk;

		f = tl_pool_tag_foot(tag);
		f->ref = tag;

		tp->cnt_inuse += tag->size;

		//TP_PROFILE_EXIT("tl pool stage 6");

	}else{

		size_t raw = tag->size;
		int idx;

		tag->size = size;
		tag->use = 1;
		tag->h_blk = f->t_blk;

		f = tl_pool_tag_foot(tag);

		tl_assert(0 == ((TL_ALIGNMENT - 1) & (uintptr_t)&(f->ref)));

		//TP_PROFILE_ENTER("tl pool stage 8");

		/*
		 *	here just shield
		 * */
		f->ref = tag;

		//TP_PROFILE_EXIT("tl pool stage 8");

		TP_PROFILE_ENTER("tl pool stage 9");

		{
			tp->cnt_inuse += size;
		}

		nt = (tl_pool_tag_t *)((uintptr_t)f + tl_pool_tag_tailS);
		nt->size = raw - tag->size;
		nt->use = 0;

		TP_PROFILE_EXIT("tl pool stage 9");

		f = tl_pool_tag_foot(nt);

		f->ref = nt;

		f->t_blk = tag->h_blk;

		idx = tl_pool_size2idx(nt->size);

		tl_pool_add_tag(tp, idx, nt);

	}

	return (void *)addr;
}

static inline int tl_pool_tag_edge(tl_pool_t *tp, tl_pool_tag_t *tag){

	uintptr_t edgl = (uintptr_t)tag - sizeof(tl_pool_blk_t);
	uintptr_t edgr = (uintptr_t)tag + tag->size;
	uintptr_t blk = (uintptr_t)(tag->h_blk);
	int ret = 0;

	if(NULL != tp) {}

	if(edgl >= blk) {
		if(edgr <= (blk + ((tl_pool_blk_t *)blk)->size)){
			if(edgl == blk) { ret |= 1; }
			if(edgr == (blk + ((tl_pool_blk_t *)blk)->size)) { ret |= 2; }
			return ret;
		}
	}

    tl_assert(false);

	//tl_assert(false, tl_error("call tl_pool_tag_edge failed info [%X:%X:%X:%X]\n", blk, ((tl_pool_blk_t *)blk)->size + blk, (uintptr_t)tag, tag->size + (uintptr_t)tag));

	return -1;
}

static inline void tl_pool_rm_blk(tl_pool_t *tp, tl_pool_blk_t *blk){

#if 0

	tl_pool_blk_t *c = tp->blk.nxt;
	tl_pool_blk_t *p = NULL;

	while(NULL != c && c != blk){
		p = c;
		c = c->nxt;
	}

	if(NULL == c){
		tl_assert(false);
	}else{
		if(NULL == p){
			tp->blk.nxt = c->nxt;
		}else{
			p->nxt = c->nxt;
		}
	}

#else

	tl_pool_lock(tp);

	if(blk == &tp->blk) { tl_assert(false); }

	blk->pre->nxt = blk->nxt;
	blk->nxt->pre = blk->pre;

#endif	

	tp->cnt_alloc -= blk->size;
	tp->cnt_blk --;

	tl_pool_unlock(tp);
}

static inline void tl_pool_add_blk(tl_pool_t *tp, tl_pool_blk_t *blk){

	tl_pool_lock(tp);

	tl_pool_blk_t *c = tp->blk.nxt;

	blk->nxt = c;

	c->pre = blk;
	blk->pre = &tp->blk;

	tp->blk.nxt = blk;

	tp->cnt_blk ++;
	tp->cnt_alloc += blk->size;

	tl_pool_unlock(tp);
}

static void tl_pool_tag_free(tl_pool_t *tp, tl_pool_tag_t *tag){

	tl_pool_tag_t *l = NULL;
	tl_pool_tag_t *r = NULL;
	tl_pool_tag_t *f = tl_pool_tag_foot(tag);
	tl_pool_blk_t *blk = tag->h_blk;
	char lu, ru;

	f->ref = tag;

	while(true){

		int ret = tl_pool_tag_edge(tp, tag);

		l = NULL;
		r = NULL;

		tl_assert(f->ref == tag);

		if(-1 == ret){
			tl_assert(false);
		}

		if(0 == ret){
			r = (tl_pool_tag_t *)((uintptr_t)f + tl_pool_tag_tailS);
			l = (tl_pool_tag_t *)((uintptr_t)tag - tl_pool_tag_tailS);
			l = l->ref;
		}else if(1 == ret){	/* left */
			r = (tl_pool_tag_t *)((uintptr_t)f + tl_pool_tag_tailS);
		}else if(2 == ret){	/* right */
			l = (tl_pool_tag_t *)((uintptr_t)tag - tl_pool_tag_tailS);
			l = l->ref;
		}else if(3 == ret){			
			if(blk != &tp->blk){
				tl_pool_rm_blk(tp, blk);

#if !(TL_SUPPORT_MMAP)
				free(blk);
#else
				munmap(blk, blk->size);
#endif
			}else{
				tag->use = 0;
				f = tl_pool_tag_foot(tag);
				f->ref = tag;
				f->t_blk = blk;
				tl_pool_add_tag(tp, tl_pool_size2idx(tag->size), tag);
			}
			break;
		}

		lu = (NULL == l) ? 1 : l->use;
		ru = (NULL == r) ? 1 : r->use;

#if TL_ASSERT_ON && TL_DEBUG_ON
		if(NULL != l){

			if(0 == l->use){
				tl_assert(blk == tl_pool_tag_foot(l)->t_blk, 
						{
						tl_error("[%X:%d:%d]\n", (uintptr_t)blk, times, ret);
						tl_pool_tag_info(l);
						tl_info("\n");
						});
			}else{
				tl_assert(blk == l->h_blk, 
						{
						tl_error("[%X:%d:%d]\n", (uintptr_t)blk, times, ret);
						tl_pool_tag_info(l);
						tl_info("\n");
						});
			}
		}

		if(NULL != r){

			if(0 == r->use){
				tl_assert(blk == tl_pool_tag_foot(r)->t_blk, 
						{
						tl_error("[%X:%d:%d]\n", (uintptr_t)blk, times, ret);
						tl_pool_tag_info(r);
						tl_info("\n");
						});
			}else{
				tl_assert(blk == r->h_blk, 
						{
						tl_error("[%X:%d:%d]\n", (uintptr_t)blk, times, ret);
						tl_pool_tag_info(r);
						tl_info("\n");
						});
			}
		}
#endif

		if(1 == lu && 1 == ru){

			tag->use = 0;
			f = tl_pool_tag_foot(tag);
			f->ref = tag;
			f->t_blk = blk;

			tl_pool_add_tag(tp, tl_pool_size2idx(tag->size), tag);
			break;
		}

		if(0 == lu && 0 == ru){

			tl_pool_rm_tag(tp, l);
			tl_pool_rm_tag(tp, r);

			l->size += tag->size;
			l->size += r->size;
			tag = l;

		}else if(1 == lu){

			tl_pool_rm_tag(tp, r);

			tag->size += r->size;

		}else if(1 == ru){

			tl_pool_rm_tag(tp, l);

			l->size += tag->size;
			tag = l;
		}

		tag->use = 0;
		tag->h_blk = blk;

		f = tl_pool_tag_foot(tag);
		f->ref = tag;
	}
}

static void *tl_pool_alloc_large(tl_pool_t *tp, size_t size){

	tl_pool_blk_t *blk = NULL;
	tl_pool_tag_t *tag = NULL;

	size += sizeof(tl_pool_blk_t);
	size = tl_align(TL_ALIGNMENT, size);

#if !(TL_SUPPORT_MMAP)
	blk = (tl_pool_blk_t *)tl_memalign(TL_ALIGNMENT, size);
#else
	blk = (tl_pool_blk_t *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

	if(NULL == blk){

		tl_pool_lock(tp);

		tp->cnt_l_fail ++;

		tl_pool_unlock(tp);

		return NULL;
	}else{

		blk->nxt = NULL;
		blk->size = size;

		tag = (tl_pool_tag_t *)((uintptr_t)blk + sizeof(tl_pool_blk_t));
		tag->use = 1;
		tag->size = size - sizeof(tl_pool_blk_t);
		tag->pre = tag;
		tag->h_blk = blk;

		tl_pool_tag_t *f = tl_pool_tag_foot(tag);
		f->ref = tag;

		tl_pool_add_blk(tp, blk);

		tl_pool_lock(tp);

		tp->cnt_inuse += tag->size;
		tp->cnt_l_hit ++;

		tl_pool_unlock(tp);

		return (void *)((uintptr_t)tag + tl_pool_tag_headS);
	}
}

tl_pool_t *tl_pool_init(){

	tl_pool_t	 *tp = NULL;
	tl_pool_tag_t *tt = NULL;
	tl_pool_tag_t *th = NULL;
	size_t size;
	int idx;

	size = tl_pagesize;

#if !(TL_SUPPORT_MMAP)
	tp = (tl_pool_t *)tl_memalign(TL_ALIGNMENT, size);
#else
	tp = (tl_pool_t *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

	if(NULL == tp){
		return NULL;
	}

	memset(tp->cnt, 0, sizeof(tp->cnt));
	memset(tp->bkt, 0, sizeof(tp->bkt));

#if TL_POOL_DEBUG_ON
	memset(tp->cnt_tag, 0, sizeof(tp->cnt_tag));
#endif

	tp->cnt_blk = 1;
	tp->cnt_alloc += size;

#if	TL_POOL_DEBUG_ON
	tp->pro = tl_profile_init(0);
	if(NULL == tp->pro){
		tl_warn("pool profile init failed\n");
	}
#endif

	size -= sizeof(tl_pool_t);

	th = (tl_pool_tag_t *)((uintptr_t)tp + sizeof(tl_pool_t));

	th->pre  = th;
	th->use  = 0;
	th->size = size;
	th->suc  = th;

	tt = tl_pool_tag_foot(th);

	tt->ref  = th;
	tt->t_blk = &(tp->blk);

	idx = tl_pool_size2idx(size);

	if(TL_POOL_MAX_INDEX == idx){
		tl_assert(false);
	}else{
		tl_pool_add_tag(tp, idx, th);
	}

	tp->blk.nxt = &tp->blk;
	tp->blk.pre = &tp->blk;
	tp->blk.size = th->size + sizeof(tl_pool_blk_t);

#if TL_SUPPORT_MULTI

	int ret = pthread_mutexattr_init(&tp->attr);
	if(0 != ret){
		tl_error("[%s] failed since [%s]\n", "pthread_mutexattr_init", strerror(ret));
		tl_pool_destroy(tp);
		return NULL;
	}

	ret = pthread_mutexattr_settype(&tp->attr, PTHREAD_MUTEX_ERRORCHECK);

	if(0 != ret){
		tl_error("[%s] failed since [%s]\n", "pthread_mutexattr_settype", strerror(ret));
		tl_pool_destroy(tp);
		return NULL;
	}

	ret = pthread_mutex_init(&tp->mutex, &tp->attr);

	while(0 != ret){
		switch (ret){
			case EAGAIN:
				continue;
			case ENOMEM:
			case EPERM:
			case EBUSY:
			case EINVAL:
			default:
				tl_error("[%s] failed since [%s]\n", "pthread_mutex_init", strerror(ret));
				tl_pool_destroy(tp);
				return NULL;
		}
	}
#endif

	return tp;
}

/*
 * FIXME
 *		tl_pool_destroy don't support multi-thread
 * */

void tl_pool_destroy(tl_pool_t *tp){

	void *p = tp;
	tl_pool_blk_t *s = &tp->blk;
	tl_pool_blk_t *blk = NULL;

#if TL_SUPPORT_MULTI
	pthread_mutexattr_destroy(&tp->attr);
	pthread_mutex_destroy(&tp->mutex);
#endif

#if TL_POOL_DEBUG_ON
	if(NULL != tp->pro){
		tl_profile_destroy(tp->pro);
	}
#endif

	for(blk = tp->blk.nxt; /* void */; p = blk, blk = blk->nxt){

#if !(TL_SUPPORT_MMAP)
		free(p);
#else
		if(p == (void *)tp){
			munmap(p, ((tl_pool_blk_t *)p)->size + tl_offsetof(tl_pool_t, blk));
		}else{
			munmap(p, ((tl_pool_blk_t *)p)->size);
		}
#endif

		if(s == blk){
			break;
		}
	}
}

void *tl_pool_alloc(tl_pool_t *tp, size_t acts){

	if(0 == acts) { return NULL; }

	acts = tl_align(TL_ALIGNMENT, acts);

	tl_pool_blk_t *blk = NULL;
	tl_pool_tag_t *tag = NULL;
	void * addr;
	int idx;
	size_t size = acts;

	tp->cnt_totalA ++;

	size += tl_pool_tag_unuseS;

	size = tl_max(size, tl_pool_tag_thresh);

	idx = tl_pool_size2idx(size);

	if(TL_POOL_MAX_INDEX == idx){

		return tl_pool_alloc_large(tp, size);

	}else{

		if(TL_POOL_MAX_INDEX - 1 > idx){
			idx ++;
		}

L_tryagain:

		tl_pool_lock(tp);

		if(TL_POOL_MAX_INDEX <= idx){
			tag = NULL;
		}else{
			tag = tp->bkt[idx];
		}

		for(; NULL == tag && TL_POOL_MAX_INDEX > idx; idx ++){
			tag = tp->bkt[idx];
		}

		tl_pool_unlock(tp);

		if(NULL != tag){

			addr = tl_pool_tag_alloc(tp, tag, size);

			if(NULL != addr){
				tp->cnt_s_hit_1 ++;
				return addr;
			}else{

				tl_assert(idx == TL_POOL_MAX_INDEX - 1, tl_error("idx is [%d]\n", idx));

				tag = tag->suc;

				TP_PROFILE_ENTER("tl pool loop last tag");

				while(tag != tp->bkt[idx]){

					addr = tl_pool_tag_alloc(tp, tag, size);

					if(NULL != addr){
						tp->cnt_s_hit_2 ++;

						TP_PROFILE_EXIT("tl pool loop last tag");

						return addr;
					}
					tag = tag->suc;
				}

				TP_PROFILE_EXIT("tl pool loop last tag");

				idx ++;
				goto L_tryagain;
			}

		}else{

			TP_PROFILE_ENTER("tl pool stage 0");

			tl_pool_tag_t *f = NULL;

			size_t rs = size;

			rs = rs;

			idx = tl_pool_size2idx(size + sizeof(tl_pool_blk_t));

			if(TL_POOL_THRESH_INDEX > idx){
				size = tl_pagesize;
			}else if(TL_POOL_MAX_INDEX > idx){
				size = tl_align(tl_pagesize, size + sizeof(tl_pool_blk_t));
				//size = 1 << (idx + TL_POOL_BASE_INDEX);
			}else{
				size += sizeof(tl_pool_blk_t);
			}

			TP_PROFILE_EXIT("tl pool stage 0");

			TP_PROFILE_ENTER("tl pool stage 1");

			tl_assert((rs + sizeof(tl_pool_blk_t)) <= size, tl_error("[%zd:%zd:%d]\n", rs, size, idx));

#if !(TL_SUPPORT_MMAP)
			blk = (tl_pool_blk_t *)tl_memalign(TL_ALIGNMENT, size);
#else
			blk = (tl_pool_blk_t *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

			TP_PROFILE_EXIT("tl pool stage 1");

			if(NULL == blk){

				//TP_PROFILE_ENTER("tl pool stage 2");

				if(TL_POOL_THRESH_INDEX > idx){

					size = 1 << (idx + TL_POOL_BASE_INDEX);

#if !(TL_SUPPORT_MMAP)
					blk = (tl_pool_blk_t *)tl_memalign(TL_ALIGNMENT, size);
#else
					blk = (tl_pool_blk_t *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

					if(NULL == blk){

						tl_pool_lock(tp);

						tp->cnt_s_fail_2 ++;

						tl_pool_unlock(tp);

						//TP_PROFILE_EXIT("tl pool stage 2");

						return NULL;
					}
				}else{

					tl_pool_lock(tp);

					tp->cnt_s_fail_1 ++;

					tl_pool_unlock(tp);

					//TP_PROFILE_EXIT("tl pool stage 2");

					return NULL;
				}
			}

			TP_PROFILE_ENTER("tl pool stage 3");

			{
				blk->size = size;
				blk->nxt = NULL;
				tl_pool_add_blk(tp, blk);
			}

			TP_PROFILE_EXIT("tl pool stage 3");

			TP_PROFILE_ENTER("tl pool stage 4");

			{
				size -= sizeof(tl_pool_blk_t);

				tag = (tl_pool_tag_t *)((uintptr_t)blk + sizeof(tl_pool_blk_t));

				idx = tl_pool_size2idx(size);

				tag->use = 0;
				tag->size = size;
				tag->pre = tag;
				tag->suc = tag;

				f = tl_pool_tag_foot(tag);

				f->ref = tag;
				f->t_blk = blk;

				tl_pool_add_tag(tp, idx, tag);
			}

			TP_PROFILE_EXIT("tl pool stage 4");

			TP_PROFILE_ENTER("tl pool stage 5");

			size = acts + tl_pool_tag_unuseS;
			size = tl_max(size, tl_pool_tag_thresh);

			addr = tl_pool_tag_alloc(tp, tag, size);

			TP_PROFILE_EXIT("tl pool stage 5");

			tl_assert(NULL != addr, tl_error("[%zd:%zd:%zd:%zd]\n", blk->size, tag->size, size, acts));

			tp->cnt_s_hit_3 ++;

			return addr;
		}
	}
}

void *tl_pool_calloc(tl_pool_t *tp, size_t n, size_t size){
	void *addr = tl_pool_alloc(tp, size * n);
	if(NULL != addr){
		memset(addr, 0, size * n);
	}
	return addr;
}

void *tl_pool_realloc(tl_pool_t *tp, void *addr, size_t size){

	void * na = NULL;
	tl_pool_tag_t *tag;

	tag = (tl_pool_tag_t *)((uintptr_t)addr - tl_pool_tag_headS);

	if(0 == size){
		if(NULL != addr){
			tl_pool_free(tp, addr);
		}
		return NULL;
	}else{
		na = tl_pool_alloc(tp, size);
		if(NULL == na){
			return addr;
		}else{
			if(NULL != addr){
				memcpy(na, addr, tl_min(size, tag->size));
			}
			return na;
		}
	}
}

void tl_pool_free(tl_pool_t *tp, void *addr){

	tl_pool_tag_t *c = NULL;
	tl_pool_tag_t *f = NULL;

	if(NULL == addr) { return; }

	c = (tl_pool_tag_t *)((uintptr_t)addr - tl_pool_tag_headS);

	f = tl_pool_tag_foot(c);

	tl_assert(1 == c->use);
	tl_assert(f->ref == c);

	tp->cnt_inuse -= c->size;

	if(TL_POOL_MAX_INDEX == tl_pool_size2idx(c->size)){

		tl_pool_blk_t *blk = (tl_pool_blk_t *)((uintptr_t)c - sizeof(tl_pool_blk_t));
		tl_pool_rm_blk(tp, blk);

#if !(TL_SUPPORT_MMAP)
		free(blk);
#else
		munmap(blk, blk->size);
#endif
	}else{
		tl_pool_tag_free(tp, c);
	}
}

uintptr_t tl_pool_static(tl_pool_t *tp, size_t idx){
	if(sizeof(tp->cnt) <= idx){
		return -1;
	}
	return tp->cnt[idx];
}

#if TL_POOL_DEBUG_ON

uintptr_t tl_pool_get_tag(tl_pool_t *tp, size_t idx){
	return tp->cnt_tag[idx];
}

tl_profile_t *tl_pool_getpro(tl_pool_t *tp){
	return tp->pro;
}

void tl_pool_traverse(tl_pool_t *tp){
	tl_pool_blk_t *blk = &(tp->blk);
	tl_pool_tag_t *tag = NULL;
	tl_pool_tag_t *f = NULL;
	int idx = 0;
	uintptr_t use = 0;
	uintptr_t unuse = 0;

	for(; NULL != blk; idx ++){

		tl_info( "[%d]'s [BLK] [%X] with size [%d]\n", idx, (uintptr_t)blk, blk->size);

		tag = tl_pool_tag_first(blk);

		if(true){
			do{
				if(0 == tag->use){
					f = tl_pool_tag_foot(tag);
				}else{
					f = tl_pool_tag_foot(tag);
				}

				if(1 == tag->use) { use ++; }
				else { unuse ++; }

				tag = tl_pool_tag_nxt(blk, tag);
			}while(NULL != tag);
		}

		blk = blk->nxt;
	}

	tl_trace( "total used tag [%d] unuse tag [%d]\n", use, unuse);

	for(idx = 0; idx < TL_POOL_MAX_INDEX; idx ++){
		tag = tp->bkt[idx];
		if(NULL != tag){
			do{
				//tl_trace( "[%d]'s [unuse TAG] [%X] with size [%d]\n", idx, (uintptr_t)tag, tag->size);
				tag = tag->suc;
			}while(tag != tp->bkt[idx]);
		}
	}
}

void tl_pool_check(tl_pool_t *tp, int line, void *t){
	tl_pool_blk_t *blk = &(tp->blk);
	tl_pool_tag_t *tag = NULL;
	tl_pool_tag_t *f = NULL;

	for(; NULL != blk;){

		tag = tl_pool_tag_first(blk);

		if(true){
			do{
				if(0 == tag->use){
					f = tl_pool_tag_foot(tag);
					tl_assert(blk == f->t_blk, 
							{
							tl_trace("in line [%d] call check\n", line);
							if(t) tl_pool_tag_info((tl_pool_tag_t *)t);
							tl_trace("\n");
							});
				}else{
					tl_assert(blk == tag->h_blk, 
							{
							tl_trace("in line [%d] call check\n", line);
							if(t) tl_pool_tag_info((tl_pool_tag_t *)t);
							tl_trace("\n");
							});
				}

				tag = tl_pool_tag_nxt(blk, tag);
			}while(NULL != tag);
		}

		blk = blk->nxt;
	}
}

#endif
