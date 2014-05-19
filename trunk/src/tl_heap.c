
#include "tl_pool.h"
#include "tl_heap.h"

#define hp_lchild(i)	(((i) << 1) + 1)
#define hp_parent(i)	((i - 1) >> 1)
#define hp_inc  128

struct hp_dyn_t;

#define hp_ele_type2(s, p, v) \
    struct hp_dyn_t { uint8_t b[(s)]; } *v;\
v = (struct hp_dyn_t *)(p);

#define hp_ele_type(hp, v)\
    hp_ele_type2(hp->hp_ele_size, hp->hp_cont, v)

struct tl_heap_t{

    size_t hp_ele_size;
    size_t hp_ele_cnt;
    keycmpHandle hp_kc;
    bool hp_raft;

    void * hp_cont;
    size_t hp_cnt;
    size_t hp_used;

    tl_pool_t *hp_pool;
    getIndexHandle getidx;
    void *_d;
};

static void hp_down(tl_heap_t *hp, size_t idx){

    hp_ele_type(hp, array);

    struct hp_dyn_t ele;
    ele = array[idx];

    size_t tmp = idx;
    size_t c = hp_lchild(idx);
    int ret;

    while(c < hp->hp_used){

        if(c < hp->hp_used - 1){

            ret = hp->hp_kc(array + c, array + c + 1);

            if(hp->hp_raft && 0 > ret){
                c++;
            }else if(!hp->hp_raft && 0 < ret){
                c ++;
            }
        }

        ret = hp->hp_kc(array + c, &ele);

        if(hp->hp_raft && 0 > ret){
            break;
        }else if (!hp->hp_raft && 0 < ret){
            break;
        }

        array[tmp] = array[c];
        
        if(hp->getidx){
            hp->getidx(array + tmp, tmp, hp->_d);
        }

        tmp = c;
        c = hp_lchild(c);
    }

    if(idx != tmp){
        
        array[tmp] = ele;

        if(hp->getidx){
            hp->getidx(array + tmp, tmp, hp->_d);
        }
    }
}

static void hp_up(tl_heap_t *hp, size_t idx){

    hp_ele_type(hp, array);

    struct hp_dyn_t ele;
    ele = array[idx];

    size_t tmp = idx;
    size_t p = hp_parent(idx);
    int ret;

    while(0 < tmp){

        ret = hp->hp_kc(array + p, &ele);

        if(hp->hp_raft && 0 < ret){
            break;
        }else if (!hp->hp_raft && 0 > ret){
            break;
        }

        array[tmp] = array[p];

        if(hp->getidx){
            hp->getidx(array + tmp, tmp, hp->_d);
        }

        tmp = p;
        p = hp_parent(p);
    }

    if(idx != tmp){
        array[tmp] = ele;

        if(hp->getidx){
            hp->getidx(array + tmp, tmp, hp->_d);
        }
    }
}

tl_heap_t *tl_heap_init(tl_pool_t *tp,
        size_t ele_size, 
        size_t ele_cnt, 
        keycmpHandle h, 
        bool raft)
{
    tl_heap_t *hp = NULL;

    if(NULL == tp){
        hp = (tl_heap_t *)malloc(sizeof(tl_heap_t));
    }else{
        hp = (tl_heap_t *)tl_pool_alloc(hp->hp_pool, sizeof(tl_heap_t));
    }

    if(NULL == hp){
        return NULL;
    }

    hp->hp_pool = tp;

    hp->hp_ele_size = ele_size;
    hp->hp_ele_cnt = ele_cnt;
    hp->hp_kc = h;
    hp->hp_raft = raft;

    hp->hp_cont = NULL;
    hp->hp_cnt = 0;
    hp->hp_used = 0;

    hp->getidx = NULL;

    return hp;
}

void tl_heap_destroy(tl_heap_t *hp){
    if(NULL != hp->hp_pool){
        tl_pool_free(hp->hp_pool, hp->hp_cont);
        tl_pool_free(hp->hp_pool, hp);
    }else{
        free(hp->hp_cont);
        free(hp);
    }
}

size_t tl_heap_size(tl_heap_t *hp){
    return hp->hp_used;
}

int tl_heap_add(tl_heap_t *hp, const void *v){

    if(hp->hp_used == hp->hp_cnt){
        if(hp->hp_cnt < hp->hp_ele_cnt){
            hp->hp_cnt = tl_min(hp->hp_ele_cnt, hp->hp_cnt + hp_inc);
            if(hp->hp_pool){
                hp->hp_cont = tl_pool_realloc(hp->hp_pool, hp->hp_cont, hp->hp_cnt * hp->hp_ele_size);
            }else{
                hp->hp_cont = realloc(hp->hp_cont, hp->hp_cnt * hp->hp_ele_size);
            }
            if(NULL == hp->hp_cont){
                return -1;
            }
        }else{
            return -1;
        }
    }

    hp_ele_type(hp, array);
    array[hp->hp_used ++] = *(struct hp_dyn_t *)v;

    hp_up(hp, hp->hp_used - 1);
    return 0;
}

void tl_heap_add_and_replace(tl_heap_t *hp, const void *v){

    if(0 < hp->hp_used){
        hp_ele_type(hp, array);
        array[0] = *(struct hp_dyn_t *)v;

        hp_down(hp, 0);
    }

}

const void *tl_heap_top(tl_heap_t *hp){
    if(0 < hp->hp_used){
        return hp->hp_cont;
    }
    return NULL;
}

int tl_heap_pop(tl_heap_t *hp, void *v /*NULL*/){

    hp_ele_type(hp, array);

    if(0 < hp->hp_used){
        if(NULL != v){
            *(struct hp_dyn_t *)v = array[0];
        }
        hp->hp_used --;
        array[0] = array[hp->hp_used];
        hp_down(hp, 0);
        return 0;
    }
    return -1;
}

void tl_heap_adjust(void *cont, size_t ele_size, keycmpHandle kc, bool raft, size_t b, size_t e){

    /*
     *  adjust b ~ (e - 1)
     * */

    hp_ele_type2(ele_size, cont, array);

    tl_heap_t hp = {
        ele_size,
        e - b,
        kc,
        raft,
        array + b,
        e - b,
        e - b,
        NULL,
        NULL,
        NULL,
    };

    size_t c = hp_parent(e - 1);

    if(e <= b || (e - b) <= 1) { return; }

    for(; 1; c --){

        hp_down(&hp, c);
        if(b == c){
            break;
        }
    }
}

#if 0

void tl_heap_down(void *cont, size_t ele_size, keycmpHandle kc, getIndexHandle getidx, bool raft, size_t b, size_t e, void *_d){

    /*
     * array range from b ~ e - 1
     * */

    hp_ele_type2(ele_size, cont, array);

    tl_heap_t hp = {
        ele_size,
        e - b,
        kc,
        raft,
        array + b,
        e - b,
        e - b,
        NULL,
        getidx,
        _d
    };

    hp_down(&hp, b);

}

void tl_heap_up(void *cont, size_t s, size_t ele_size, keycmpHandle kc, getIndexHandle getidx, bool raft, size_t b, void *_d){

    hp_ele_type2(ele_size, cont, array);

    tl_heap_t hp = {
        ele_size,
        s,
        kc,
        raft,
        array,
        s,
        s,
        NULL,
        getidx,
        _d
    };

    hp_up(&hp, b);
}

#else

void tl_heap_down(void *cont, size_t s, size_t ele_size, keycmpHandle kc, getIndexHandle getidx, bool raft, size_t idx, void *_d){

    /*
     * array range from b ~ e - 1
     * */

    hp_ele_type2(ele_size, cont, array);

    tl_heap_t hp = {
        ele_size,
        s,
        kc,
        raft,
        array,
        s,
        s,
        NULL,
        getidx,
        _d
    };

    hp_down(&hp, idx);

}

void tl_heap_up(void *cont, size_t s, size_t ele_size, keycmpHandle kc, getIndexHandle getidx, bool raft, size_t idx, void *_d){

    hp_ele_type2(ele_size, cont, array);

    tl_heap_t hp = {
        ele_size,
        s,
        kc,
        raft,
        array,
        s,
        s,
        NULL,
        getidx,
        _d
    };

    hp_up(&hp, idx);
}

#endif
