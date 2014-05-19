#include <sys/time.h>
#include <unistd.h>
#include <tl_assert.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "tl_sort.h"
#include "tl_btree2.h"
#include "tl_btree.h"
#include "tl_rbtree.h"
#include "tl_profile.h"
#include "tl_alg.h"
#include "tl_queue.h"

#define min(a, b)	((a) < (b) ? (a) : (b))
#define max(a, b)	((a) < (b) ? (b) : (a))

//#define __USE_SPEC
//#define __LOCATE

tl_keyval_t *array = NULL;
tl_profile_t *afx_pro = NULL;

int keycmp(const void * p1, const void * p2){
	intptr_t v1 = (intptr_t)p1;
	intptr_t v2 = (intptr_t)p2;

	if(v1 > v2) return 1;
	if(v1 < v2) return -1;
	return 0;
}

int qcmp(const void *p1, const void *p2){
    int pi1 = *(const int *)p1;
    int pi2 = *(const int *)p2;

    if(pi1 > pi2) return 1;
    if(pi1 < pi2) return -1;
    return 0;
}

int qcmp2(const void *p1, const void *p2){
    intptr_t pi1 = *(intptr_t *)(p1);
    intptr_t pi2 = *(intptr_t *)(p2);

    if(pi1 > pi2) return 1;
    if(pi1 < pi2) return -1;
    return 0;
}

void sortTest(int cnt){
	tl_profile_t *pro = tl_profile_init(0);
	if(NULL == pro) { tl_assert(false); }

    int *arrayh = (int *)malloc(sizeof(int) * cnt);
    int *arrayq = (int *)malloc(sizeof(int) * cnt);
    int i = 0;
    struct timeval s, e;

    for (i = 0; i < cnt; i ++){
        arrayh[i] = rand();
    }
    memcpy(arrayq, arrayh, sizeof(int) *cnt);
    
    ttl_trace(  "sort begin ...\n");

    sort_config_t scq = { arrayq, sizeof(int), cnt, qcmp };
    sort_config_t sch = { arrayh, sizeof(int), cnt, qcmp };
    
    gettimeofday(&s, NULL);
	
	tl_profile_start(pro, "q_sort");
    tl_q_sort(&scq);
	tl_profile_end(pro, "q_sort");

    gettimeofday(&e, NULL);
    e.tv_sec = e.tv_sec - s.tv_sec;
	if(e.tv_usec < s.tv_usec){
		e.tv_sec -= 1;
		e.tv_usec = 1000000 + e.tv_usec - s.tv_usec;
	}else{
		e.tv_usec = e.tv_usec - s.tv_usec;
	}

    ttl_trace(  "qsort used <%d:%d> when sort <%d> eles\n", (int)e.tv_sec, (int)e.tv_usec, cnt);
    
    gettimeofday(&s, NULL);

	tl_profile_start(pro, "h_sort");
    tl_h_sort(&sch);
	tl_profile_end(pro, "h_sort");

    gettimeofday(&e, NULL);
    e.tv_sec = e.tv_sec - s.tv_sec;
	if(e.tv_usec < s.tv_usec){
		e.tv_sec -= 1;
		e.tv_usec = 1000000 + e.tv_usec - s.tv_usec;
	}else{
		e.tv_usec = e.tv_usec - s.tv_usec;
	}

    ttl_trace(  "hsort used <%d:%d> when sort <%d> eles\n", (int)e.tv_sec, (int)e.tv_usec, cnt);

    for(i = 0; i < cnt; i ++){
        tl_assert(arrayh[i] == arrayq[i]);
    }

    free(arrayh);
    free(arrayq);
	

	const tl_profile_entry_t *entry = tl_profile_getEntry(pro);
	ttl_trace(  "[func : cpu : real-sec : real-usec]\n");
	while(NULL != entry){
		ttl_trace(  "[%s:%f:%d:%d]\n", entry->item->func, ((double) (entry->ut.cpu) / CLOCKS_PER_SEC), (int)entry->ut.real.tv_sec, (int)entry->ut.real.tv_usec);
		entry = entry->next;
	}

	tl_profile_destroy(pro);
}

//#define __USE_SPEC

int btreeTest(int fk, int cnt){
	int i;
	int ret = -1;
	size_t act_cnt = 0;

	tl_btree_root_t *root = NULL;
	tl_rbtree_root_t *rb = NULL;
	tl_bplustree_root_t *bp = NULL;

	tl_profile_t *pro = tl_profile_init(0);

	tl_assert(pro);
	
	int bp_fork = 3 + rand() % fk;
	int b_fork = 2 + rand() % fk;
	
	tl_profile_start(pro, "tl_bplustree_init");
	bp = tl_bplustree_init(&(tl_bplustree_config_t){bp_fork, keycmp});
	tl_profile_end(pro, "tl_bplustree_init");

	if(NULL == bp){
		ttl_trace(  "tl_bplustree init failed ...\n");
		return -1;
	}

	tl_profile_start(pro, "tl_btree_init");
	root = tl_btree_init(&(tl_btree_config_t){b_fork, qcmp});
	tl_profile_end(pro, "tl_btree_init");

	if(NULL == root){
		ttl_trace(  "btree init failed ...\n");
		return -1;
	}
	
	tl_profile_start(pro, "tl_rbtree_init");
	rb = tl_rbtree_init(&(rb_config_t){keycmp});
	tl_profile_end(pro, "tl_rbtree_init");

	if(NULL == rb) {
		ttl_trace(  "tl_rbtree init failed ...\n");
		return -1;
	}

	ttl_trace(  "[ BPLUSTREE ] with fork <%d>\n", bp_fork);
	ttl_trace(  "[ BTREE ]     with fork <%d>\n", b_fork);

#ifndef __USE_SPEC

#ifdef __LOCATE
	cnt = cnt % 10;
	if(0 == cnt) cnt = 10;
#endif

	uintptr_t mask = cnt * cnt;
	
	tl_profile_start(pro, "random");

	if(0 == mask) { mask = cnt; }
	for(i = 0; i < cnt; i ++){
		array[i].key = (tl_key_t)(rand() % mask);
		array[i].val = (tl_val_t)(rand());
	}

	tl_profile_end(pro, "random");

	ttl_trace(  "<-- generate random data over \n");

#endif
	
	tl_profile_start(afx_pro, "all insert");

	for(i = 0; i < cnt; i ++){

		tl_profile_start(pro, "tl_btree_insert");
		ret = tl_btree_insert(root, array + i, false);
		tl_profile_end(pro, "tl_btree_insert");

		if(0 == ret){
			tl_profile_start(pro, "tl_rbtree_insert");
			tl_assert(0 == tl_rbtree_insert(rb, array + i, false));
			tl_profile_end(pro, "tl_rbtree_insert");

			tl_profile_start(pro, "tl_bplustree_insert");
			tl_assert(0 == tl_bplustree_insert(bp, array + i, false));
			tl_profile_end(pro, "tl_bplustree_insert");

			act_cnt ++;

#ifdef __LOCATE
			ttl_trace(  "{(tl_key_t)%d,(tl_val_t)%d}, ", (intptr_t)array[i].key, (intptr_t)array[i].val);
#endif
			//ttl_trace(  "{%d,%d}\n", (intptr_t)array[i].key, (intptr_t)array[i].val);
#ifdef __USE_SPEC
			ttl_trace(  "insert [%d:%d] over\n", (intptr_t)array[i].key, (intptr_t)array[i].val);
#endif
		}
	}
	ttl_trace(  "<-- insert [%d:%d] test pass\n", cnt, act_cnt);

	tl_profile_end(afx_pro, "all insert");

	tl_profile_start(pro, "tl_btree_check");
	ret = tl_btree_check(root);
	tl_profile_end(pro, "tl_btree_check");

	if(0 != ret){
		tl_assert(false);
	}

	if(act_cnt != tl_rbtree_getcnt(rb)){
		ttl_trace(  "[ ERROR ] --  actual insert <%d> keys\n", act_cnt);
		ttl_trace(  "[ ERROR ] --  btree  has    <%d> keys\n", tl_btree_getcnt(root));
		ttl_trace(  "[ ERROR ] --  tl_rbtree has    <%d> keys\n", tl_rbtree_getcnt(rb));
		ttl_trace(  "[ ERROR ] --  tl_bplustree has <%d> keys\n", tl_bplustree_getcnt(bp));

		tl_assert(false);
	}
	
	if(act_cnt != tl_btree_getcnt(root)){
		ttl_trace(  "[ ERROR ] --  actual insert <%d> keys\n", act_cnt);
		ttl_trace(  "[ ERROR ] --  btree  has    <%d> keys\n", tl_btree_getcnt(root));
		ttl_trace(  "[ ERROR ] --  tl_rbtree has    <%d> keys\n", tl_rbtree_getcnt(rb));
		ttl_trace(  "[ ERROR ] --  tl_bplustree has <%d> keys\n", tl_bplustree_getcnt(bp));

		tl_assert(false);
	}

	if(act_cnt != tl_bplustree_getcnt(bp)){
		ttl_trace(  "[ ERROR ] --  actual insert <%d> keys\n", act_cnt);
		ttl_trace(  "[ ERROR ] --  btree  has    <%d> keys\n", tl_btree_getcnt(root));
		ttl_trace(  "[ ERROR ] --  tl_rbtree has    <%d> keys\n", tl_rbtree_getcnt(rb));
		ttl_trace(  "[ ERROR ] --  tl_bplustree has <%d> keys\n", tl_bplustree_getcnt(bp));

		tl_assert(false);
	}
	
	if(true){

		tl_profile_start(pro, "forward iteration");

		tl_rbtree_entry_t *e1 = alloca(tl_rbtree_entrySize());
		tl_btree_entry_t *e2 = alloca(tl_btree_entrySize());

		int ret1;
		int ret2;

		tl_keyval_t kv1;
		tl_keyval_t kv2;
		
		ret1 = tl_rbtree_first(rb, e1);
		ret2 = tl_btree_first(root, e2);

		tl_assert(ret1 == ret2);
		if(0 == ret1){
			do{
				tl_rbtree_extract(e1, &kv1);
				tl_btree_extract(e2, &kv2);
				
				//ttl_trace(  "tl_rbtree: <%d:%d>\n", (intptr_t)kv1.key, (intptr_t)kv1.val);
				//ttl_trace(  "btree : <%d:%d>\n", (intptr_t)kv2.key, (intptr_t)kv2.val);

				if(kv1.key != kv2.key || kv1.val != kv2.val){
					ttl_trace(  "[ ERROR ] -- tl_rbtree: <%d:%d>\n", (intptr_t)kv1.key, (intptr_t)kv1.val);
					ttl_trace(  "[ ERROR ] -- btree : <%d:%d>\n", (intptr_t)kv2.key, (intptr_t)kv2.val);
					tl_assert(false);
				}

				ret1 = tl_rbtree_next(e1);
				ret2 = tl_btree_next(e2);

				if(ret1 != ret2){
					ttl_trace(  "[ ERROR ] -- ret1 = <%d> ret2 = <%d>\n", ret1, ret2);
					tl_assert(false);
				}
			}while(0 == ret1);
		}

		tl_profile_end(pro, "forward iteration");

		ttl_trace(  "<-- forward iterator test pass\n");
	}

	if(true){

		#ifdef __USE_SPEC
			tl_btree_show(root);
		#endif
		tl_profile_start(pro, "reverse iteration");

		tl_rbtree_entry_t *e1 = alloca(tl_rbtree_entrySize());
		tl_btree_entry_t *e2 = alloca(tl_btree_entrySize());

		int ret1;
		int ret2;

		tl_keyval_t kv1;
		tl_keyval_t kv2;
		
		ret1 = tl_rbtree_last(rb, e1);
		ret2 = tl_btree_last(root, e2);

		tl_assert(ret1 == ret2);
		if(0 == ret1){
			do{
				tl_rbtree_extract(e1, &kv1);
				tl_btree_extract(e2, &kv2);

#ifdef __USE_SPEC
				ttl_trace(  "tl_rbtree: <%d:%d>\n", (intptr_t)kv1.key, (intptr_t)kv1.val);
				ttl_trace(  "btree : <%d:%d>\n", (intptr_t)kv2.key, (intptr_t)kv2.val);
#endif

				if(kv1.key != kv2.key || kv1.val != kv2.val){
					ttl_trace(  "[ ERROR ] -- tl_rbtree: <%d:%d>\n", (intptr_t)kv1.key, (intptr_t)kv1.val);
					ttl_trace(  "[ ERROR ] -- btree : <%d:%d>\n", (intptr_t)kv2.key, (intptr_t)kv2.val);
					tl_assert(false);
				}

				ret1 = tl_rbtree_prev(e1);
				ret2 = tl_btree_prev(e2);

				if(ret1 != ret2){
					ttl_trace(  "[ ERROR ] -- ret1 = <%d> ret2 = <%d>\n", ret1, ret2);
					tl_assert(false);
				}
			}while(0 == ret1);
		}

		tl_profile_end(pro, "reverse iteration");

		ttl_trace(  "<-- reverse iterator test over\n");
	}

	if(true){
		
		tl_profile_start(afx_pro, "query & erase");

		tl_keyval_t kv;
		for(i = 0; i < cnt; i ++){
			kv.key = array[i].key;
			kv.val = (tl_val_t)0;
			
			tl_profile_start(pro, "tl_btree_query");
			ret = tl_btree_query(root, &kv);
			tl_profile_end(pro, "tl_btree_query");

			if(0 == ret){
				tl_assert(kv.val == array[i].val);
				kv.val = (tl_val_t)0;
				
				tl_profile_start(pro, "tl_rbtree_query");
				tl_assert(0 == tl_rbtree_query(rb, &kv));
				tl_profile_end(pro, "tl_rbtree_query");
				
				tl_assert(kv.val == array[i].val);

				tl_profile_start(pro, "tl_bplustree_query");
				tl_assert(0 == tl_bplustree_query(bp, &kv));
				tl_profile_end(pro, "tl_bplustree_query");
				
				tl_assert(kv.val == array[i].val);
				
				tl_profile_start(pro, "tl_btree_erase");
				tl_assert(0 == tl_btree_erase(root, &kv));
				tl_profile_end(pro, "tl_btree_erase");

				tl_profile_start(pro, "tl_rbtree_erase");
				tl_assert(0 == tl_rbtree_erase(rb, &kv));
				tl_profile_end(pro, "tl_rbtree_erase");

				tl_profile_start(pro, "tl_bplustree_erase");
				tl_assert(0 == tl_bplustree_erase(bp, &kv));
				tl_profile_end(pro, "tl_bplustree_erase");

				act_cnt --;
			}
		}

		tl_assert(0 == act_cnt);
		tl_assert(0 == tl_rbtree_getcnt(rb));
		tl_assert(0 == tl_btree_getcnt(root));

		ttl_trace(  "<-- erase key test pass\n");

		tl_profile_end(afx_pro, "query & erase");
	}
	
	tl_profile_start(pro, "tl_btree_destroy");
	tl_btree_destroy(root);
	tl_profile_end(pro, "tl_btree_destroy");

	tl_profile_start(pro, "tl_rbtree_destroy");
	tl_rbtree_destroy(rb);
	tl_profile_end(pro, "tl_rbtree_destroy");

	tl_profile_start(pro, "tl_bplustree_destroy");
	tl_bplustree_destroy(bp);
	tl_profile_end(pro, "tl_bplustree_destroy");

	tl_rbtree_entry_t *entry = alloca(tl_rbtree_entrySize());
	tl_profile_item_t *item = NULL;
	tl_keyval_t kv;

	if(-1 != tl_profile_first(pro, entry)){

		ttl_trace(  "\t[RUN TIME TAKE STATIC INFO]\n");

		do{
			tl_rbtree_extract(entry, &kv);
			item = (tl_profile_item_t *)kv.val;
			ttl_trace(  "[%-18s:%-8d:%-8f:%3d:%8d]\n", item->func, item->cnt_call, ((double) (item->total.cpu) / CLOCKS_PER_SEC), (int)item->total.real.tv_sec, (int)item->total.real.tv_usec);
		}while(-1 != tl_profile_next(entry));
	}

	tl_profile_destroy(pro);
	return 0;
}

void bsTest(int cnt){
	int *array = (int *)malloc(sizeof(int) * cnt);
	if(NULL == array) { ttl_trace(  "tl_bsearch test failed ...\n"); return; }
	int i;
	int expect;
	for(i = 0; i < cnt; i ++) { array[i] = i << i; }
	for(i = 0; i < cnt; i ++) { ttl_trace(  "%d ", array[i]); }	ttl_trace(  "\n");

	expect = cnt >> 2;
	ttl_trace(  "tl_bsearch search <%d> return <%d>\n", expect, tl_bsearch(&(sort_config_t){array, sizeof(intptr_t), cnt, qcmp}, (tl_key_t)expect, true));
	
	expect = cnt << 1;
	ttl_trace(  "tl_bsearch search <%d> return <%d>\n", expect, tl_bsearch(&(sort_config_t){array, sizeof(intptr_t), cnt, qcmp}, (tl_key_t)expect, true));
	
	expect = -1;
	ttl_trace(  "tl_bsearch search <%d> return <%d>\n", expect, tl_bsearch(&(sort_config_t){array, sizeof(intptr_t), cnt, qcmp}, (tl_key_t)expect, true));

	expect = cnt;
	ttl_trace(  "tl_bsearch search <%d> return <%d>\n", expect, tl_bsearch(&(sort_config_t){array, sizeof(intptr_t), cnt, qcmp}, (tl_key_t)expect, true));

	free(array);
}

void queueTest(int cnt){
	tl_queue_t *q = tl_queue_init(sizeof(int));
	int i;
	for(i = 0; i < cnt; i ++){
		tl_queue_push(q, &i);
	}
	int idx = 0;
	ttl_trace(  "queue size is <%d>\n", tl_queue_size(q));
	while(0 < tl_queue_size(q)){
		i = *(int *)tl_queue_top(q);	
		tl_assert(i == idx);
		idx ++;
		tl_queue_pop(q);
	}
	ttl_trace(  "\n");
	tl_assert(idx == cnt);
	tl_queue_destroy(q);
}

int main(int argc, const char *argv[]){

	afx_pro = tl_profile_init(0);

	tl_profile_start(afx_pro, __FUNCTION__);

	int cnt = 1024;

	if(1 < argc){
		cnt = atoi(argv[1]);
		if(0 == cnt){
			cnt = 1024;
		}
	}
	srand(time(NULL));
	
	//sortTest(cnt);	
	
    //bsTest(cnt);
	//queueTest(cnt);
	//goto __return;
	
	int i = 0;
	int fk = 4;
	int totalkey;
	
#ifndef __USE_SPEC
	array = (tl_keyval_t *)malloc(sizeof(tl_keyval_t) * (512 * 512));
	if(NULL == array) { return 0; }
#else
	tl_keyval_t spec[] = {{(tl_key_t)20,(tl_val_t)144451521}, {(tl_key_t)87,(tl_val_t)1228576615}, {(tl_key_t)45,(tl_val_t)391397935}, {(tl_key_t)52,(tl_val_t)175720675}, {(tl_key_t)35,(tl_val_t)966613030}, {(tl_key_t)70,(tl_val_t)579309322}, {(tl_key_t)53,(tl_val_t)1042470509}, {(tl_key_t)11,(tl_val_t)1421046931}, {(tl_key_t)80,(tl_val_t)2070996692}, {(tl_key_t)75,(tl_val_t)934366892}};

	cnt = 1;
	array = spec;
#endif
	
        cnt = 10;
	for(i = 0; i < cnt; i ++){
		fk = rand() % 512;

		if(0 == fk) { fk = 32; }

#ifdef __USE_SPEC
		totalkey = sizeof(spec) / sizeof(tl_keyval_t);
		fk = 3;
#else
		totalkey = rand() % (fk * fk);

                totalkey = 10;
#endif

		ttl_trace(  "--------- <%d:%d> <%d:%d> ---------\n", cnt, i, totalkey, fk);
		btreeTest(fk, totalkey);
	}

#ifndef __USE_SPEC
	free(array);
#endif

	//__return:

	tl_profile_end(afx_pro, __FUNCTION__);
	
	ttl_trace(  "\t\t[ TOTAL STATIC INFO ] \n");

	tl_rbtree_entry_t *entry = alloca(tl_rbtree_entrySize());
	
	if(0 == tl_profile_first(afx_pro, entry)){
		tl_keyval_t kv;
		tl_profile_item_t *item = NULL;
		do{
			tl_rbtree_extract(entry, &kv);
			item = (tl_profile_item_t *)kv.val;
			ttl_trace(  "[%-18s:%-8d:%-8f:%3d:%8d]\n", item->func, item->cnt_call, ((double) (item->total.cpu) / CLOCKS_PER_SEC), (int)item->total.real.tv_sec, (int)item->total.real.tv_usec);
		}while(0 == tl_profile_next(entry));
	}else{
	}

	tl_profile_destroy(afx_pro);
    return 0;
}
