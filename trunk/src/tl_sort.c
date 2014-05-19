
/*
 *	  Copyright (C) yangnian
 *	  Email:  ydyangnian@163.com
 * */

#include "tl_sort.h"
#include "tl_alg.h"

#define alias(name)			 ((intptr_t)(name))
#define vpalias(name)		   ((void *)(name))
#define cvpalias(name)		  ((const void *)(name))

#define sc_cmp(psc)			 (psc->cmp)
#define sc_at(psc, idx)		 (alias((psc)->array) + (idx) * (psc)->eleSize)
#define sc_next(psc, ptr)	   (alias(ptr) + (psc)->eleSize)
#define sc_prev(psc, ptr)	   (alias(ptr) - (psc)->eleSize)

#define auto_cmp			(sc_cmp(psc))
#define LT(ptr1, ptr2) (-1 == auto_cmp(cvpalias(ptr1), cvpalias(ptr2)))
#define GT(ptr1, ptr2) ( 1 == auto_cmp(cvpalias(ptr1), cvpalias(ptr2)))
#define EQ(ptr1, ptr2) ( 0 == auto_cmp(cvpalias(ptr1), cvpalias(ptr2)))
#define LE(ptr1, ptr2) ( 1  > auto_cmp(cvpalias(ptr1), cvpalias(ptr2)))
#define GE(ptr1, ptr2) (-1  < auto_cmp(cvpalias(ptr1), cvpalias(ptr2)))

#define type_swap(type, ptr1, ptr2) do{\
    *(type *)(ptr1) ^= *(type *)(ptr2);\
    *(type *)(ptr2) ^= *(type *)(ptr1);\
    *(type *)(ptr1) ^= *(type *)(ptr2);\
    (ptr1) = (void *)(alias(ptr1) + sizeof(type));\
    (ptr2) = (void *)(alias(ptr2) + sizeof(type));\
}while(0)

static void swap(void *ptr1, void *ptr2, int size){
    tl_assert(ptr1 != ptr2);
    size_t left = size;
    for(; left >= sizeof(intptr_t); left -= sizeof(intptr_t)){
        type_swap(int, ptr1, ptr2);
    }

    tl_assert(1 == sizeof(char));

    for(; left; left -= sizeof(char)){
        type_swap(char, ptr1, ptr2);
    }
}

int tl_bsearch(sort_config_t *psc, tl_key_t key, bool b_lowerbound)
{
    int l = -1;
    int h = psc->eleCnt;
    int m = -1;

    while((l + 1) != h){
        m = (l + h) >> 1;
        if(GE(sc_at(psc, m), key)) { h = m; }
        else{ l = m; }
    }

    if((size_t)h == psc->eleCnt || !EQ(sc_at(psc, h), key)){
        if(b_lowerbound) { return h; }
        else { return -1; }
    }
    return h;
}

void tl_i_sort(sort_config_t *psc){

    intptr_t ptr1;
    intptr_t ptr2;
    intptr_t end = sc_at(psc, psc->eleCnt);
    intptr_t begin = sc_at(psc, 0);
    intptr_t tmp = (intptr_t)alloca(psc->eleSize);

    for(ptr1 = sc_at(psc, 1); ptr1 < end; ptr1 = sc_next(psc, ptr1)){
        memcpy(vpalias(tmp), vpalias(ptr1), psc->eleSize);
        for(ptr2 = ptr1; ptr2 > begin; ptr2 = sc_prev(psc, ptr2)){
            if(GT(tmp, sc_prev(psc, ptr2))){
                break;
            }
            memcpy(vpalias(ptr2), vpalias(sc_prev(psc, ptr2)), psc->eleSize);
        }
        if(ptr2 != ptr1){
            memcpy(vpalias(ptr2), vpalias(tmp), psc->eleSize);
        }
    }
}

static int partition(sort_config_t *psc){
    if(1 == psc->eleCnt) { tl_assert(false); }
    intptr_t ptr = sc_at(psc, 0);
    intptr_t ptrl = sc_next(psc, ptr);
    intptr_t ptrh = sc_at(psc, psc->eleCnt - 1);

    while(ptrl <= ptrh){
        while(ptrl <= ptrh && LE(ptr, ptrh)){
            ptrh = sc_prev(psc, ptrh);
        }
        while(ptrl <= ptrh && GE(ptr, ptrl)){
            ptrl = sc_next(psc, ptrl);
        }
        if(ptrl < ptrh){
            swap(vpalias(ptrl), vpalias(ptrh), psc->eleSize);
        }
    }
    if(ptr != ptrh){
        swap(vpalias(ptr), vpalias(ptrh), psc->eleSize);
    }
    return (ptrh - ptr) / psc->eleSize;
}

void tl_q_sort(sort_config_t *psc){
    int pos = -1;
    sort_config_t scl = *psc;
    sort_config_t sch = *psc;

    if(32 < psc->eleCnt){
        pos = partition(psc);
        scl.eleCnt = pos;
        sch.array  = vpalias(sc_at(psc, pos + 1));
        sch.eleCnt = psc->eleCnt - pos - 1;

        tl_q_sort(&scl);
        tl_q_sort(&sch);
    }else{
        tl_i_sort(psc);
    }
    return ;
}

#define parent(i)	   (((i) - 1) >> 1)
#define lchild(i)	   (((i) << 1) + 1)

static void adjust(sort_config_t *psc, int b, int e){
    int c;
    intptr_t cur = sc_at(psc, b);
    intptr_t cp;
    for(c = lchild(b); c < e;){
        cp = sc_at(psc, c);
        if(c < (e - 1) && LT(cp, sc_next(psc, cp))){
            c ++;
            cp = sc_next(psc, cp);
        }
        if(GE(cur, cp)){
            break;
        }
        swap(vpalias(cur), vpalias(cp), psc->eleSize);
        cur = cp;
        c = lchild(c);
    }
}

void tl_h_sort(sort_config_t *psc){
    if(2 > psc->eleCnt) { return ; }

    int idx;
    for(idx = parent(psc->eleCnt - 1); 0 <= idx; idx --){
        adjust(psc, idx, psc->eleCnt);
    }
    swap(vpalias(sc_at(psc, psc->eleCnt - 1)), vpalias(sc_at(psc, 0)), psc->eleSize);

    for(idx = psc->eleCnt - 1; 0 < idx; idx --){
        adjust(psc, 0, idx);
        if(0 == (idx - 1)) { break; }
        swap(vpalias(sc_at(psc, idx - 1)), vpalias(sc_at(psc, 0)), psc->eleSize);
    }
    return ;
}
