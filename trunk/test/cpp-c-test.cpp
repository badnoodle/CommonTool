#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>

#include <map>

#include "tl_btree2.h"
#include "tl_sort.h"
#include "tl_btree.h"
#include "tl_rbtree.h"
#include "tl_profile.h"
#include "tl_alg.h"
#include "tl_queue.h"
#include "tl_pool.h"
#include "tl_trace.h"
#include "tl_atomic.h"
#include "tl_lock.h"
#include "tl_heap.h"

#ifdef LINUX
#include <alloca.h>
#endif

using namespace std;

#define min(a, b)	((a) < (b) ? (a) : (b))
#define max(a, b)	((a) < (b) ? (b) : (a))

//#define __USE_SPEC
//#define __LOCATE

tl_pool_t *afx_pool = NULL;
tl_keyval_t *array = NULL;
tl_profile_t *afx_pro = NULL;
uintptr_t *afx_mem = NULL;
uintptr_t *afx_ptr = NULL;


void atomicTest();

void cb(int sig){
    if(SIGABRT == sig){
        tl_trace(  "get the signal ABRT\n");
        if(NULL != afx_pool){
            tl_pool_static(afx_pool, 0);
            //tl_pool_traverse(afx_pool);
        }
    }else{
        tl_trace(  "get another signal\n");
    }
}

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
    intptr_t pi1 = *(const intptr_t *)p1;
    intptr_t pi2 = *(const intptr_t *)p2;

    if(pi1 > pi2) return 1;
    if(pi1 < pi2) return -1;
    return 0;
}

void sortCmp(size_t cnt){
    size_t i;
    intptr_t * array[4];
    int mask;

    mask = cnt * cnt;
    if(0 == mask) { mask = cnt; }
    for(i = 0; i < sizeof(array) / sizeof(intptr_t *); i ++){
        array[i] = (intptr_t *)malloc(sizeof(intptr_t) * cnt);
        if(NULL == array[i]) { tl_assert(false); }
    }

    for(i = 0; i < cnt; i ++){
        array[0][i] = (rand() % mask);
    }

    for(i = 1; i < sizeof(array) / sizeof(intptr_t *); i ++){
        memcpy(array[i], array[0], sizeof(intptr_t) * cnt);
    }

    for(i = 0; i < cnt; i ++){
        if(rand() % cnt < (cnt * 0.2)){
            array[3][i] = rand();
        }
    }

    sort_config_t scq = { array[1], sizeof(intptr_t), cnt, qcmp2 };
    sort_config_t sch = { array[2], sizeof(intptr_t), cnt, qcmp2 };
    sort_config_t scbs = { array[1], sizeof(intptr_t), cnt, qcmp2 };

    tl_profile_t *pro = tl_profile_init(0);

    tl_profile_start(pro, "sort c");
    qsort(array[0], cnt, sizeof(intptr_t), qcmp2);
    tl_profile_end(pro, "sort c");

    tl_profile_start(pro, "sort q");
    tl_q_sort(&scq);
    tl_profile_end(pro, "sort q");

    tl_profile_start(pro, "sort h");
    tl_h_sort(&sch);
    tl_profile_end(pro, "sort h");

    if(true){
        int idx;
        intptr_t *ptr = NULL;
        size_t c1 = 0;
        size_t c2 = 0;

        tl_profile_start(pro, "bs self");
        for(i = 0; i < cnt; i ++){
            idx = tl_bsearch(&scbs, (tl_key_t)(&array[3][i]), false);
            if(-1 != idx){
                c1 ++;
                tl_assert(array[1][idx] == array[3][i]);
            }
        }
        tl_profile_end(pro, "bs self");

        tl_profile_start(pro, "bs std");
        for(i = 0; i < cnt; i ++){
            ptr = (intptr_t *)bsearch(&array[3][i], array[2], cnt, sizeof(intptr_t), qcmp2);
            if(NULL != ptr){
                c2 ++;
                tl_assert(*ptr == array[3][i]);
            }
        }
        tl_profile_end(pro, "bs std");

        if(c1 != c2){
            tl_trace(  "\t[ ERROR ] <c1 = %d, c2 = %d>\n", c1, c2);
            tl_assert(false);
        }
    }

#if 0
    tl_trace(  "final0 data is:\n");
    for(i = 0; i < cnt; i ++){
        tl_trace(  "%d ", array[0][i]);
    }
    tl_trace(  "\n");

    tl_trace(  "final1 data is:\n");
    for(i = 0; i < cnt; i ++){
        tl_trace(  "%d ", array[1][i]);
    }
    tl_trace(  "\n");

    tl_trace(  "final2 data is:\n");
    for(i = 0; i < cnt; i ++){
        tl_trace(  "%d ", array[2][i]);
    }
    tl_trace(  "\n");

    tl_trace(  "final3 data is:\n");
    for(i = 0; i < cnt; i ++){
        tl_trace(  "%d ", array[3][i]);
    }
    tl_trace(  "\n");
#endif

    for(size_t k = 0; k < cnt; k ++){
        tl_assert(array[1][k] == array[2][k]);
        tl_assert(array[0][k] == array[1][k]);
        //tl_assert(array[0][k] == array[2][k]);
        //tl_assert(array[0][k] == array[2][k]);
    }

    tl_profile_item_t *item = NULL;
    tl_rbtree_entry_t *entry = (tl_rbtree_entry_t *)alloca(tl_rbtree_entrySize());
    tl_keyval_t kv;

    if(-1 != tl_profile_first(pro, entry)){

        tl_trace(  "\t[SORT RUN TIME TAKE STATIC INFO]\n");

        do{
            tl_rbtree_extract(entry, &kv);
            item = (tl_profile_item_t *)kv.val;

            tl_trace(  "[%-20s:%-8d:%-8f:%3d:%8d]\n", item->func, item->cnt_call, ((double) (item->total.cpu) / CLOCKS_PER_SEC), (int)item->total.real.tv_sec, (int)item->total.real.tv_usec);
        }while(-1 != tl_profile_next(entry));
    }

    tl_profile_destroy(pro);

    for(i = 0; i < sizeof(array) / sizeof(intptr_t *); i ++){
        free(array[i]);
    }
}

void compare(int cnt){

    size_t act_cnt1 = 0, act_cnt2 = 0;
    int i;
    int mask;
    int ret;
    bool b_qe = false;

    map<intptr_t, intptr_t> cppm;

    rb_config_t conf = {keycmp};
    tl_rbtree_root_t *root = tl_rbtree_init(&conf);;
    tl_profile_t *pro = tl_profile_init(0);

    //tl_profile_start(pro, "random");

    mask = cnt * cnt;
    if(0 == mask) { mask = cnt; }
    for(i = 0; i < cnt; i ++){
        array[i].key = (tl_key_t)(rand() % mask);
        array[i].val = (tl_val_t)(rand());
    }

    //tl_profile_end(pro, "random");

    tl_profile_start(pro, "insert tl_rbtree");
    for(i = 0; i < cnt; i ++){
        ret = tl_rbtree_insert(root, array + i, false);
        if(0 == ret){
            act_cnt1 ++;
        }
    }
    tl_profile_end(pro, "insert tl_rbtree");

    tl_assert(0 == tl_rbtree_check(root));

    tl_profile_start(pro, "insert map");
    for(i = 0; i < cnt; i ++){
        if(cppm.end() == cppm.find((intptr_t)array[i].key)){
            cppm[(intptr_t)array[i].key] = (intptr_t)array[i].val;
            act_cnt2 ++;
        }
    }
    tl_profile_end(pro, "insert map");

    tl_trace(  "\t[MAP:%d] \n\t[RBTREE:%d] \n\t[ACT_CNT1:%d] \n\t[ACT_CNT2:%d]\n", cppm.size(), tl_rbtree_getcnt(root), act_cnt1, act_cnt2);

    if(act_cnt1 != cppm.size()) { tl_assert(false); }
    if(act_cnt2 != tl_rbtree_getcnt(root)) { tl_assert(false); }
    tl_assert(act_cnt1 == act_cnt2);

    tl_rbtree_entry_t *entry = (tl_rbtree_entry_t *)alloca(tl_rbtree_entrySize());
    map<intptr_t, intptr_t>::iterator iter;
    map<intptr_t, intptr_t>::reverse_iterator riter;
    tl_keyval_t kv;

    if(true){
        tl_profile_start(pro, "reverse iter tl_rbtree");
        int ret = tl_rbtree_last(root, entry);
        if(0 == ret){
            do{
                ;
            }while(0 == tl_rbtree_prev(entry));
        }
        tl_profile_end(pro, "reverse iter tl_rbtree");

        tl_profile_start(pro, "reverse iter map");
        riter = cppm.rbegin();
        if(cppm.rend() != riter){
            do{
                riter ++;
            }while(cppm.rend() != riter);
        }
        tl_profile_end(pro, "reverse iter map");
    }

    if(true){
        tl_profile_start(pro, "forward iter tl_rbtree");
        int ret = tl_rbtree_first(root, entry);
        if(0 == ret){
            do{
                ;
            }while(0 == tl_rbtree_next(entry));
        }
        tl_profile_end(pro, "forward iter tl_rbtree");

        tl_profile_start(pro, "forward iter map");
        iter = cppm.begin();
        if(cppm.end() != iter){
            do{
                iter ++;
            }while(cppm.end() != iter);
        }
        tl_profile_end(pro, "forward iter map");
    }

    if(true){
        int ret = tl_rbtree_last(root, entry);
        riter = cppm.rbegin();
        if(0 == ret){
            tl_assert(cppm.rend() != riter);
            do{
                tl_assert(cppm.rend() != riter);
                tl_rbtree_extract(entry, &kv);

                tl_assert((intptr_t)kv.key == riter->first);
                tl_assert((intptr_t)kv.val == riter->second);

                riter ++;
            }while(0 == tl_rbtree_prev(entry));
            tl_assert(cppm.rend() == riter);
        }
    }

    if(true){
        int ret = tl_rbtree_first(root, entry);
        iter = cppm.begin();
        if(0 == ret){
            tl_assert(cppm.end() != iter);
            do{
                tl_assert(cppm.end() != iter);
                tl_rbtree_extract(entry, &kv);

                tl_assert((intptr_t)kv.key == iter->first);
                tl_assert((intptr_t)kv.val == iter->second);

                iter ++;
            }while(0 == tl_rbtree_next(entry));
            tl_assert(cppm.end() == iter);
        }
    }

    if(!b_qe){
        int cnt_query1 = 0;
        int cnt_query2 = 0;

        tl_profile_start(pro, "query tl_rbtree");
        for(i = 0; i < cnt; i ++){
            kv.key = array[i].key;
            kv.val = (tl_val_t)0;

            ret = tl_rbtree_query(root, &kv);
            if(0 == ret){
                cnt_query1 ++;
                //tl_assert(kv.val == array[i].val);
            }
        }
        tl_profile_end(pro, "query tl_rbtree");

        tl_profile_start(pro, "query map");
        for(i = 0; i < cnt; i ++){
            iter = cppm.find((intptr_t)array[i].key);
            if(cppm.end() != iter){
                cnt_query2 ++;
            }
        }
        tl_profile_end(pro, "query map");

        tl_assert(cnt_query1 == cnt_query2);
    }

    if(!b_qe){
        tl_profile_start(pro, "erase tl_rbtree");
        for(i = 0; i < cnt; i ++){
            kv.key = array[i].key;
            kv.val = (tl_val_t)0;

            ret = tl_rbtree_erase(root, &kv);
            if(0 == ret){
                tl_assert(kv.val == array[i].val);
            }
        }
        tl_profile_end(pro, "erase tl_rbtree");

        tl_profile_start(pro, "erase map");
        for(i = 0; i < cnt; i ++){
            cppm.erase((intptr_t)array[i].key);
        }
        tl_profile_end(pro, "erase map");
    }

    if(b_qe){
        tl_profile_start(pro, "query&erase tl_rbtree");
        for(i = 0; i < cnt; i ++){
            kv.key = array[i].key;
            kv.val = (tl_val_t)0;

            ret = tl_rbtree_query(root, &kv);
            if(0 == ret){
                tl_assert(kv.val == array[i].val);
                tl_assert(0 == tl_rbtree_erase(root, &kv));
            }
        }
        tl_profile_end(pro, "query&erase tl_rbtree");

        tl_profile_start(pro, "query&erase map");
        for(i = 0; i < cnt; i ++){
            iter = cppm.find((intptr_t)array[i].key);
            if(cppm.end() != iter){
                cppm.erase(iter);
            }
        }
        tl_profile_end(pro, "query&erase map");
    }

    tl_profile_item_t *item = NULL;

    if(-1 != tl_profile_first(pro, entry)){
        struct timeval total;
        double average;

        tl_trace(  "\t[RBTREE RUN TIME TAKE STATIC INFO]\n");

        do{
            tl_rbtree_extract(entry, &kv);
            item = (tl_profile_item_t *)kv.val;

            total.tv_usec = 1000000 * item->total.real.tv_sec + item->total.real.tv_usec;
            average = (double)total.tv_usec / act_cnt1;

            tl_trace(  "[%-20s:%-8d:%-8f:%3d:%8d:%10f]\n", item->func, item->cnt_call, ((double) (item->total.cpu) / CLOCKS_PER_SEC), (int)item->total.real.tv_sec, (int)item->total.real.tv_usec, average);
        }while(-1 != tl_profile_next(entry));
    }

    tl_profile_destroy(pro);
    tl_rbtree_destroy(root);
}

void memTest(int cnt){
    int idx;
    tl_rbtree_entry_t *entry = (tl_rbtree_entry_t *)alloca(tl_rbtree_entrySize());
    tl_profile_t *pro = tl_profile_init(0);
    tl_pool_t *tp = tl_pool_init();

    uintptr_t rp_freed = 0;
    uintptr_t rp_large = 0;
    uintptr_t rp_std = 0;

    uintptr_t act_freed1 = 0;
    uintptr_t act_freed2 = 0;

    uintptr_t failed1 = 0;
    uintptr_t failed2 = 0;

    uintptr_t suc1 = 0;
    uintptr_t suc2 = 0;

    if(NULL == pro){
        tl_trace(  "tl_profile init failed \n");
        return;
    }

    uintptr_t total = 0;
    uintptr_t tm = 0XFFFFFFFF;

    for(idx = 0; idx < cnt; idx ++){

#if 1

        if(3 >= rand() % 10000){
            afx_mem[idx] = rand() % (1 << 26);
            rp_large ++;
        }else{
            afx_mem[idx] = rand() % (1 << 10);
            rp_std ++;
        }

        if(0 == afx_mem[idx]){
            afx_mem[idx] = (2 << (1 + (rand() % 5)));
        }

        if((tm - total) <= afx_mem[idx]){
            //tl_warn("[%d:%d] [%u - %u = %u < %u]\n", cnt, idx, tm ,total, tm - total, afx_mem[idx]);
        }
        total += afx_mem[idx];

#else

        afx_mem[idx] = rand() % tl_pagesize;

#endif

        if(40 >= rand() % 100){
            afx_mem[idx] |= (0X80000000);
            rp_freed ++;
        }
    }

    tl_trace(  "mem size init over with total size [%u:%u]\n", total, tm);

#if 1

    if(NULL == tp){
        tl_trace(  "tl_pool_init failed \n");
        return ;
    }else{

        afx_pool = tp;

        tl_profile_start(pro, "tl pool");

        for(idx = 0; idx < cnt; idx ++){
            size_t size = afx_mem[idx];
            bool b_f = 0X80000000 == (size & 0X80000000) ? true : false;
            size = size & (~ 0X80000000);

            tl_profile_start(pro, "tl_pool_alloc");

            afx_ptr[idx] = (uintptr_t)tl_pool_alloc(tp, size);

            tl_profile_end(pro, "tl_pool_alloc");

            if(0 != afx_ptr[idx]){

                suc1 ++;

                if(b_f){

                    tl_profile_start(pro, "tl_pool_free");

                    tl_pool_free(tp, (void *)afx_ptr[idx]);

                    tl_profile_end(pro, "tl_pool_free");

                    act_freed1 ++;
                }
            }else{
                failed1 ++;
            }
        }

        tl_profile_end(pro, "tl pool");
    }

    for(idx = 2; idx < TL_POOL_CNT_MAX; idx ++){
        tl_info("[%8u] ", tl_pool_static(tp, idx));
    }
    tl_info("\n");

#if 0
    for(idx = 0; idx < TL_POOL_MAX_BKT - 6; idx ++){
        tl_info("[%6u] ", tl_pool_get_tag(tp, idx));
    }
    tl_info("\n");
#endif

    for(idx = 0; idx < cnt; idx ++){
        size_t size = afx_mem[idx];
        bool b_f = 0X80000000 == (size & 0X80000000) ? true : false;
        if(0 != afx_ptr[idx]){
            if(!b_f){
                tl_pool_free(tp, (void *)afx_ptr[idx]);
            }
        }
    }

    for(idx = 2; idx < TL_POOL_CNT_MAX; idx ++){
        tl_info("[%8u] ", tl_pool_static(tp, idx));
    }
    tl_info("\n");

#if 0
    for(idx = 0; idx < TL_POOL_MAX_BKT; idx ++){
        tl_info("[%6u] ", tl_pool_get_tag(tp, idx));
    }
    tl_info("\n");
#endif

    if(tl_pool_static(tp, TL_POOL_CNT_ALLOC) != 4096){
        tl_assert(false);
    }

    //tl_pool_traverse(tp);

#endif

#if TL_POOL_DEBUG_ON

    tl_profile_t *tpro = tl_pool_getpro(tp);

    if(NULL != tpro && 0 == tl_profile_first(tpro, entry)){
        do{
            tl_keyval_t kv;
            tl_profile_item_t *item = NULL;

            tl_rbtree_extract(entry, &kv);
            item = (tl_profile_item_t *)kv.val;

            tl_info("%24s:%8d used %8d:%8d:%f\n", item->func, item->cnt_call, (int)item->total.real.tv_sec, (int)item->total.real.tv_usec, (double)item->total.real.tv_usec / item->cnt_call);
        }while(0 == tl_profile_next(entry));
    }

#endif

    tl_pool_destroy(tp);

    afx_pool = NULL;


#if 1
    tl_trace(  "%8s:%8s:%8s:%8s\n", "rp_large", "rp_std", "rp_free", "total");
    tl_trace(  "%8d:%8d:%8d:%8d\n", rp_large, rp_std, rp_freed, rp_large + rp_std);

    tl_trace(  "%8s:%8s:%8s:%8s\n", "act_suc", "act_failed", "act_freed", "total");
    tl_trace(  "%8d:%8d:%8d:%8d\n", suc1, failed1, act_freed1, suc1 + failed1);
#endif


#if 1

    tl_profile_start(pro, "malloc");

    for(idx = 0; idx < cnt; idx ++){

        size_t size = afx_mem[idx];
        bool b_f = 0X80000000 == (size & 0X80000000) ? true : false;
        size = size & (~ 0X80000000);

        //tl_profile_start(pro, "malloc");

        afx_ptr[idx] = (uintptr_t)malloc(size);

        //tl_profile_end(pro, "malloc");

        if(0 == afx_ptr[idx]){
            failed2 ++;
        }else{
            //free((void *)afx_ptr[idx]);
            if(0 == size) {
                failed2 ++;
            }else{
                suc2 ++;
            }
            if(b_f){

                tl_profile_start(pro, "free");

                free((void *)afx_ptr[idx]);

                tl_profile_end(pro, "free");

                act_freed2 ++;
            }
        }
    }

    for(idx = 0; idx < cnt; idx ++){
        size_t size = afx_mem[idx];
        bool b_f = 0X80000000 == (size & 0X80000000) ? true : false;
        if(0 != afx_ptr[idx]){
            if(!b_f){
                free((void *)afx_ptr[idx]);
            }
        }
    }

    tl_profile_end(pro, "malloc");

#endif

    tl_trace(  "%8d:%8d:%8d:%8d\n", suc2, failed2, act_freed2, suc2 + failed2);

    if(0 == tl_profile_first(pro, entry)){
        do{
            tl_keyval_t kv;
            tl_profile_item_t *item = NULL;

            tl_rbtree_extract(entry, &kv);
            item = (tl_profile_item_t *)kv.val;

            tl_info("%24s:%8d used %8d:%8d\n", item->func, item->cnt_call, (int)item->total.real.tv_sec, (int)item->total.real.tv_usec);
        }while(0 == tl_profile_next(entry));
    }

    tl_profile_destroy(pro);

}

void heapTest(int cnt){
    int idx;
    int ret;
    int old;

    tl_heap_t *hp = tl_heap_init(NULL, sizeof(int), cnt, qcmp, 0);

    if(NULL == hp){
        tl_trace("heap init failed\n");
        return;
    }

    for(idx = 0; idx < cnt; idx ++){
        int v = rand() % (cnt * cnt);

        afx_mem[idx] = (uintptr_t)v;

        ret = tl_heap_add(hp, (void *)&v);
        if(0 != ret){
            tl_trace("heap add [%d]'th value [%d] failed\n", idx, v);
            return;
        }else{
        }
    }

    tl_trace("add done with size [%d]\n", tl_heap_size(hp));
    
    bool bf = true;
    idx = 0;
    while(0 < tl_heap_size(hp)){

        tl_heap_adjust(afx_mem, sizeof(int), qcmp, 0, 0, cnt - idx);
        
        int *tmp = (int *)tl_heap_top(hp);

        if(!bf){
            tl_assert(old <= *tmp);
        }

        tl_assert(*tmp == (int)afx_mem[0]);

        old = *tmp;

        tl_heap_pop(hp, NULL);

        afx_mem[0] = afx_mem[cnt - 1 - idx];

        idx ++;
    }
}

int main(int argc, const char *argv[]){

    afx_pro = tl_profile_init(0);

    tl_profile_start(afx_pro, __FUNCTION__);

    int cnt = 1024;
    int i;
    int totalkey;
    int mask = 2200;

    if(1 < argc){
        cnt = atoi(argv[1]);
        if(0 == cnt){
            cnt = 1024;
        }
        if(2 < argc){
            mask = atoi(argv[2]);
            if(0 == mask) { mask = 2500; }
            if(2500 < mask) { mask = 2500; }
        }
    }
    srand(time(NULL));

    array = (tl_keyval_t *)malloc(sizeof(tl_keyval_t) * (mask * mask));
    if(NULL == array) { return 0; }
    afx_mem = (uintptr_t *)malloc(sizeof(uintptr_t) * (mask * mask));
    if(NULL == afx_mem) { return 0; }
    afx_ptr = (uintptr_t *)malloc(sizeof(uintptr_t) * (mask * mask));
    if(NULL == afx_ptr) { return 0; }

    if(SIG_ERR == signal(SIGABRT, cb)){
        tl_trace(  "call signal failed\n");
        return 0;
    }

    cnt = 4;

    for(i = 0; i < cnt; i ++){

        totalkey = rand() % (mask * mask);

        //totalkey %= 20000;

        tl_trace(  "--------- <%d:%d> <%d> ---------\n", cnt, i, totalkey);

        //heapTest(totalkey);
        //continue;

        //memTest(totalkey);
        compare(totalkey);
        sortCmp(totalkey);
        //atomicTest();
    }

    tl_profile_end(afx_pro, __FUNCTION__);

    tl_trace(  "\t\t[ TOTAL STATIC INFO ] \n");

    tl_rbtree_entry_t *entry = (tl_rbtree_entry_t *)alloca(tl_rbtree_entrySize());

    if(0 == tl_profile_first(afx_pro, entry)){
        tl_keyval_t kv;
        tl_profile_item_t *item = NULL;

        do{
            tl_rbtree_extract(entry, &kv);
            item = (tl_profile_item_t *)kv.val;
            tl_trace(  "[%-18s:%-8d:%-8f:%3d:%8d]\n", item->func, item->cnt_call, ((double) (item->total.cpu) / CLOCKS_PER_SEC), (int)item->total.real.tv_sec, (int)item->total.real.tv_usec);
        }while(0 == tl_profile_next(entry));
    }else{
        tl_log_here();
    }

    tl_profile_destroy(afx_pro);

    free(afx_mem);
    free(afx_ptr);
    free(array);

    return 0;
}

#define TL_THREAD_TEST_CNT	32
#define TL_THREAD_ATOMIC_TEST_USE	2

tl_atomic_t afx_atomic[TL_THREAD_TEST_CNT];
tl_lock_t afx_lock[TL_THREAD_TEST_CNT];

pthread_cond_t	afx_cond;
pthread_mutex_t afx_condM;
int afx_sig = 0;

typedef struct{
    int t;
    int v;
    uint32_t c;
}tl_oper_t;

void *atomicTask(void *ptr){

    tl_oper_t *oper = (tl_oper_t *)ptr;
    size_t i;
    int tmp = 0;

    pthread_mutex_lock(&afx_condM);

    while(0 == afx_sig){
        pthread_cond_wait(&afx_cond, &afx_condM);
    }

    pthread_mutex_unlock(&afx_condM);

#if 1
    char name[128];
    pthread_mutex_lock(&afx_condM);
    snprintf(name, sizeof(name) - 1, "thread [%x]", (uintptr_t)pthread_self());
    tl_profile_start(afx_pro, name);
    pthread_mutex_unlock(&afx_condM);
#endif

    while(0 < oper->c){

        for( i = 0; i < sizeof(afx_atomic) / sizeof(tl_atomic_t); i ++){
            switch (oper->t){
                case 1:

#if	(TL_THREAD_ATOMIC_TEST_USE == 0)

                    tl_atomic_fetch_sub(&afx_atomic[i], oper->v);

#elif (TL_THREAD_ATOMIC_TEST_USE == 1)
                    tl_lock_on(&afx_lock[i]);
                    afx_atomic[i] -= oper->v;
                    tl_lock_off(&afx_lock[i]);
#else
                    pthread_mutex_lock(&afx_condM);
                    afx_atomic[i] -= oper->v;
                    pthread_mutex_unlock(&afx_condM);
#endif

                    tmp -= oper->v;
                    break;

                case 0:

#if	(TL_THREAD_ATOMIC_TEST_USE == 0)

                    tl_atomic_fetch_add(&afx_atomic[i], oper->v);

#elif (TL_THREAD_ATOMIC_TEST_USE == 1)
                    tl_lock_on(&afx_lock[i]);
                    afx_atomic[i] += oper->v;
                    tl_lock_off(&afx_lock[i]);
#else
                    pthread_mutex_lock(&afx_condM);
                    afx_atomic[i] += oper->v;
                    pthread_mutex_unlock(&afx_condM);
#endif

                    tmp += oper->v;
                    break;
                default:
                    tl_assert(false);
                    break;
            }
        }

        oper->c --;
    }

#if 1
    pthread_mutex_lock(&afx_condM);
    tl_profile_end(afx_pro, name);
    pthread_mutex_unlock(&afx_condM);
#endif

    //return (void*)pthread_self();
    return (void *)tmp;
}


void *lockTask(void *p){

    pthread_mutex_lock(&afx_condM);

    while(0 == afx_sig){
        pthread_cond_wait(&afx_cond, &afx_condM);
    }

    pthread_mutex_unlock(&afx_condM);

#if 1
    char name[128];
    pthread_mutex_lock(&afx_condM);
    snprintf(name, sizeof(name) - 1, "thread [%x]", (uintptr_t)pthread_self());
    tl_profile_start(afx_pro, name);
    pthread_mutex_unlock(&afx_condM);
#endif

    tl_oper_t *oper = (tl_oper_t *)p;
    int t = 0;

    while(oper->c){

        oper->c --;

#if (TL_THREAD_TEST_USE == 1)

        tl_lock_on(&afx_lock[0]);
        t ++;
        tl_lock_off(&afx_lock[0]);

#else
        pthread_mutex_lock(&afx_condM);
        t --;
        pthread_mutex_unlock(&afx_condM);
#endif
    }

#if 1
    pthread_mutex_lock(&afx_condM);
    tl_profile_end(afx_pro, name);
    pthread_mutex_unlock(&afx_condM);
#endif

    return 0;
}

void atomicTest(){

    size_t i;
    for(i = 0; i < sizeof(afx_atomic) / sizeof(tl_atomic_t); i ++){
        tl_atomic_set(&afx_atomic[i], 0);
    }

    pthread_t pid[TL_THREAD_TEST_CNT];

    tl_oper_t oper[TL_THREAD_TEST_CNT];

    if(0 != pthread_cond_init(&afx_cond, NULL)){
        tl_error("pthread_cond_init init failed\n");
        return;
    }

    if(0 != pthread_mutex_init(&afx_condM, NULL)){
        tl_error("pthread_mutex_init init failed\n");
        return;
    }

#if	(TL_THREAD_ATOMIC_TEST_USE == 0)

    tl_trace("will use atomic oper\n");

#elif (TL_THREAD_ATOMIC_TEST_USE == 1)
    tl_trace("will use self lock to impl atomic oper\n");
#else
    tl_trace("will use pthread to impl atomic oper\n");
#endif

    for( i = 0; i < TL_THREAD_TEST_CNT; i ++){

        oper[i].t = i & 1;
        if(1 == (i & 1)){
            oper[i].v = oper[i - 1].v;
        }else{
            oper[i].v = rand() % 13;
        }
        oper[i].c = 300000;

        if(0){
            if(0 != pthread_create(&pid[i], NULL, atomicTask, &oper[i])){
                tl_error("[%d]'s atomic task init failed\n", i);
            }else{
                //tl_trace("thread [%X] init ok\n", (uintptr_t)pid[i]);
            }
        }else{

            tl_lock_init(&afx_lock[i]);

            if(0 != pthread_create(&pid[i], NULL, lockTask, &oper[i])){
                tl_error("[%d]'s atomic task init failed\n", i);
            }else{
                //tl_trace("thread [%X] init ok\n", (uintptr_t)pid[i]);
            }
        }
    }

    pthread_mutex_lock(&afx_condM);
    afx_sig = 1;
    tl_trace(" now tell all threads start ...\n");
    pthread_cond_broadcast(&afx_cond);
    pthread_mutex_unlock(&afx_condM);

    int ret;
    void *tmpL = NULL;
    for( i = 0; i < TL_THREAD_TEST_CNT; i ++){
        if(0 == (ret = pthread_join(pid[i], &tmpL))){
            tl_info("thread [%X] returned [%d]\n", (uintptr_t)pid[i], (int)tmpL);
        }else{
            tl_error("when join thread [%X] failed, since error[%d:%s]\n", (uintptr_t)pid[i], ret, strerror(ret));
        }
    }

    for(i = 0; i < TL_THREAD_TEST_CNT; i ++){
        tl_trace("atomic[%d] is [%u]\n", i, tl_atomic_get(&afx_atomic[i]));
    }

    pthread_mutex_destroy(&afx_condM);
    pthread_cond_destroy(&afx_cond);
}
