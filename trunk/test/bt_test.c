
#include <sys/time.h>

#include <tl_assert.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "tl_sort.h"
#include "tl_btree.h"
#include "tl_rbtree.h"
#include "tl_binomialheap.h"

#define __DEBUG

#ifdef __DEBUG
    #define		ttl_trace(  fmt, ...) do { printf(fmt, ##__VA_ARGS__); } while(0)
#else
    #define		ttl_trace(  fmt, ...)
#endif

#define log_here()    ttl_trace(  "now in here <%s:%d>\n", __FILE__, __LINE__)

#define min(a, b)	((a) < (b) ? (a) : (b))
#define max(a, b)	((a) < (b) ? (b) : (a))

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
	
    ttl_trace(  "now cmp <%d><%d>\n", pi1, pi2);

    if(pi1 > pi2) return 1;
    if(pi1 < pi2) return -1;
    return 0;
}

bool legalK(const void **pk){
	return (void *)-1 != *pk;
}

bool legalV(const void **pv){
	return (void *)-1 != *pv;
}

void sortTest(int cnt){
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
    tl_q_sort(&scq);
    gettimeofday(&e, NULL);
    e.tv_sec = e.tv_sec - s.tv_sec;
    e.tv_usec = e.tv_usec - s.tv_usec;

    if(e.tv_usec < 0) {e.tv_usec += 1000000; e.tv_sec -= 1;}

    ttl_trace(  "qsort used <%d:%d> when sort <%d> eles\n", (int)e.tv_sec, (int)e.tv_usec, cnt);
    
    gettimeofday(&s, NULL);
    tl_h_sort(&sch);
    gettimeofday(&e, NULL);
    e.tv_sec = e.tv_sec - s.tv_sec;
    e.tv_usec = e.tv_usec - s.tv_usec;

    if(e.tv_usec < 0) {e.tv_usec += 1000000; e.tv_sec -= 1;}

    ttl_trace(  "hsort used <%d:%d> when sort <%d> eles\n", (int)e.tv_sec, (int)e.tv_usec, cnt);

    for(i = 0; i < cnt; i ++){
        tl_assert(arrayh[i] == arrayq[i]);
    }

    free(arrayh);
    free(arrayq);
}

void bhTest(int cnt){
	binomialheap_root_t *root = bhInit((binomialheap_config_t *)(keycmp));
	if(NULL == root) { ttl_trace(  "init faield \n"); }
	int *array = (int *)malloc(sizeof(int) * cnt);
	for(int i =0 ; i < cnt; i ++){
		array[i] = rand() % 1024;
	}
}

int main(int argc, const char *argv[]){

	srand(time(NULL));

	bhTest();
	return 0;

	bt_static_t bts = bt_static_init;
        btree_config_t btc = { 6, keycmp, legalK, legalV };
	rb_config_t rbcfg = { keycmp, legalK, legalV };

	intptr_t *array = NULL;
        intptr_t *bak = NULL;
	intptr_t mask = sizeof(intptr_t) * 8 - 1;

	struct timeval tv_s, tv_e;
	
	if(3 > argc){
		ttl_trace(  "[usage] <test count> <val mask>\n");
		return 1;
	}
	
	intptr_t l = 1;
	int cnt = atoi(argv[1]);
	int act_cnt = 0;
	int i;
	intptr_t val = (intptr_t)NULL;
	bt_keyval_t kv;
	
	array = (intptr_t *)malloc(sizeof(intptr_t) * cnt * 2);
	if(NULL == array) {
		ttl_trace(  "array init failed ...\n");
		return 1;
	}

        bak = (intptr_t *)malloc(sizeof(intptr_t) * cnt * 2);

	mask = min(mask, atoi(argv[2]));
	mask = max(1, mask);
	mask = (l << mask) - 1;
	
        ttl_trace(  "mask is <%d> cnt is %d\n", mask, cnt);
	for(i = 0; i < cnt; i ++){
	    val = (rand()) & mask;
	    array[i << 1] = val;
	    array[(i << 1) + 1] = val;
	}

#if 1
        sortTest(cnt);
        return 0;
#endif

        void *ptr = (void *)0X1;

        ttl_trace(  "%X -> %X\n", (intptr_t)ptr,(intptr_t)( (intptr_t)ptr + sizeof(int)));// ptr = ptr + sizeof(int);

	struct btree_root_t *root = NULL;
	rb_root_t *rbroot = NULL;

	rbroot = rbInit(&rbcfg);
	if(NULL == rbroot){
		return 1;
	}else{
		rb_keyval_t kv;
		if(-1 == gettimeofday(&tv_s, NULL)){
			ttl_trace(  "call gettimeofday failed, since <%s>\n", strerror(errno));
			return -1;
		}
                
                intptr_t val;
		for(i = 0; i < cnt; i ++){
                        val = array[i << 1];
			kv.key = (rb_key_t)val;
			kv.val = (rb_val_t)(val);
			if(0 == rbInsert(rbroot, &kv, false)){
				//ttl_trace(  "insert <%d:%d> ok\n", (intptr_t)kv.key, (intptr_t)kv.val);
                                bak[act_cnt << 1] = val;
                                act_cnt ++;
			}else{
                            //tl_assert(false);
                            continue;
				ttl_trace(  "insert <%d:%d> failed\n", (intptr_t)kv.key, (intptr_t)kv.val);
				tl_assert(false);
			}
		}
        
		if(-1 == gettimeofday(&tv_e, NULL)){
			ttl_trace(  "call gettimeofday failed, since <%s>\n", strerror(errno));
			return -1;
		}

		ttl_trace(  "total insert <%d> eles, average time used when insert an ele <%f>\n", 
                        act_cnt, ((tv_e.tv_sec - tv_s.tv_sec) * (double)1000000.0 + tv_e.tv_usec - tv_s.tv_usec) / (double)cnt);

                rb_static_t rbs = rb_static_init;
                rbGetStatic((const rb_root_t *)rbroot, &rbs);
                //ttl_trace(  "check rbtree return <%d> and total alloc <%d> times\n", rbCheck(rbroot), rbs.cnt_totalAlloc);

                int ret;
                rb_keyval_t old;
		for(i = 0; i < act_cnt; i ++){
			kv.key = (rb_key_t)bak[i << 1];
                        old.key = kv.key;
                        ret = rbQuery(rbroot, &old, false);
                        rbErase(rbroot, &kv, true);
                        
                        if(old.key != kv.key || old.val != kv.val){
                            ttl_trace(  "erase return <%d:%d> <%d:%d>\n", 
                              (intptr_t)kv.key, (intptr_t)kv.val, (intptr_t)old.key, (intptr_t)old.val);
                            tl_assert(false);
                        }
                        #if 0
                        ret = rbCheck(rbroot);
                        ttl_trace(  "check rbtree return <%d> after erase <%d>'s key <%d:%d>\n", ret, i, (intptr_t)kv.key, (intptr_t)kv.val);
#endif
                        if(-1 == ret) {
                            tl_assert(false);
                        }
                        if(kv.key != kv.val){
                            tl_assert(false);
                        }
		}

                rbDestroy(rbroot);
                free(bak);
                free(array);
		return 0;
	}

	root = btInit(&btc);

	if(-1 == gettimeofday(&tv_s, NULL)){
		ttl_trace(  "call gettimeofday failed, since <%s>\n", strerror(errno));
		return -1;
	}

	for(i = 0; i < cnt; i ++){
		kv.key = (bt_key_t)array[i << 1];
		kv.val = (bt_val_t)array[(i << 1) + 1];
		if(-1 == btInsert(root, &kv, false)){
			//ttl_trace(  "insert <%d:%d> failed \n", (int)kv.key, (int)kv.val);
		}else{
			//ttl_trace(  "insert <%d:%d> \n", (intptr_t)kv.key, (intptr_t)kv.val);
			//array[(act_cnt << 1) + 1] = val;
			act_cnt ++;
		}
	}

	if(-1 == gettimeofday(&tv_e, NULL)){
		ttl_trace(  "call gettimeofday failed, since <%s>\n", strerror(errno));
		return -1;
	}

	ttl_trace(  "wanted insert <%d> key-vals, actually insert <%d>, average insert oper use <%f> usec\n", 
		  cnt, act_cnt, ((tv_e.tv_sec - tv_s.tv_sec) * (double)1000000.0 + tv_e.tv_usec - tv_s.tv_usec) / (double)cnt);
	
	btGetStatics(root, &bts);
	
	ttl_trace(  "[ BTREE's total key    ] = <%d>\n", bts.cnt_totalKey);
	ttl_trace(  "[ BTREE's total node   ] = <%d>\n", bts.cnt_totalNode);
	ttl_trace(  "[ BTREE's total leaf   ] = <%d>\n", bts.cnt_totalLeaf);
	ttl_trace(  "[ BTREE's total allocN ] = <%d>\n", bts.cnt_totalAllocN);
	ttl_trace(  "[ BTREE's total allocL ] = <%d>\n", bts.cnt_totalAllocL);
	ttl_trace(  "[ BTREE's total reuseN ] = <%d>\n", bts.cnt_totalReuseN);
	ttl_trace(  "[ BTREE's total reuseL ] = <%d>\n", bts.cnt_totalReuseL);


#if 0
	for(i = 0; i < (cnt); i ++){
		int idx = rand() % cnt;
		kv.key = (bt_key_t)array[idx << 1];
		kv.val = (bt_val_t)array[(idx << 1) + 1];
		btErase(root, &kv, false);
		//tl_assert(0 == btErase(root, &kv, false));
		//tl_assert(0 == btInsert(root, &kv, false));
	}
	
	ttl_trace(  "delete and insert again with total <%d> times ...\n", cnt >> 4);

	btGetStatics(root, &bts);

	ttl_trace(  "[ BTREE's total key    ] = <%d>\n", bts.cnt_totalKey);
	ttl_trace(  "[ BTREE's total node   ] = <%d>\n", bts.cnt_totalNode);
	ttl_trace(  "[ BTREE's total leaf   ] = <%d>\n", bts.cnt_totalLeaf);
	ttl_trace(  "[ BTREE's total allocN ] = <%d>\n", bts.cnt_totalAllocN);
	ttl_trace(  "[ BTREE's total allocL ] = <%d>\n", bts.cnt_totalAllocL);
	ttl_trace(  "[ BTREE's total reuseN ] = <%d>\n", bts.cnt_totalReuseN);
	ttl_trace(  "[ BTREE's total reuseL ] = <%d>\n", bts.cnt_totalReuseL);
#endif

	for(i = 0; i < act_cnt; i ++){
		kv.key = (bt_key_t)array[i << 1];
		kv.val = (bt_val_t)array[(i << 1) + 1];
		if(-1 == btQuery(root, &kv, false)){
			ttl_trace(  "when query <%d:%d> failed ...\n", (intptr_t)kv.key, (intptr_t)kv.val);
		}
	}

	ttl_trace(  "total delete node is <%d>\n", btDestroy(root));
	free(array);
	return 0;
}
