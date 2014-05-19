#include <stdlib.h>
#include <string.h>
#include "tl_queue.h"

#ifdef LINUX
#include <linux/types.h>
#endif

typedef struct tl_queue_list_t{
	void * container;
	size_t size;
	size_t s;
	size_t e;
	struct tl_queue_list_t *next;
}tl_queue_list_t;

struct tl_queue_t {
	size_t eleSize;
	size_t size;

	tl_queue_list_t *h;
	tl_queue_list_t *t;
	tl_queue_list_t *f;
};

static tl_queue_list_t *newList(tl_queue_t *q, size_t size){
	tl_queue_list_t *n = NULL;
	if(NULL != q->f){
		n = q->f;
		q->f = q->f->next;
		n->e = 0;
		n->s = 0;
		n->next = NULL;
	}else{
		n = (tl_queue_list_t *)calloc(1, sizeof(tl_queue_list_t));
		if(NULL == n) { return NULL; }
		n->size = size;
		n->container = malloc(q->eleSize * size);
		if(NULL == n->container){
			free(n);
			return NULL;
		}
	}
	return n;
}

static void freeList(tl_queue_list_t *l){
	tl_queue_list_t *n = NULL;
	while(NULL != l){
		n = l->next;
		free(l->container);
		free(l);
		l = n;
	}
}

tl_queue_t *tl_queue_init(size_t eleSize){
	tl_queue_t *q = (tl_queue_t *)calloc(1, sizeof(tl_queue_t));
	if(NULL == q) { return NULL; }
	q->eleSize = eleSize;
	return q;
}

int tl_queue_push(tl_queue_t *q, tl_queue_ele_t ele){

	tl_queue_list_t *t = q->t;

	if(NULL == t || t->e == t->size){
		if(NULL == t){
			t = newList(q, 512);
			if(NULL == t) { return -1; }
		}else{
			t->next = newList(q, 512);
			if(NULL == t->next) { return -1; }
			t = t->next;
		}
		q->t = t;
	}

	if(NULL == q->h) { q->h = t; }

	memcpy((void *)((uintptr_t)t->container + t->e * q->eleSize), ele, q->eleSize);
	t->e ++;
	q->size ++;

	return 0;
}

void tl_queue_pop(tl_queue_t *q){
	tl_queue_list_t *tmp = NULL;
	if(0 == q->size) {
		return; 
	}

	q->h->s ++;
	if(q->h->e == q->h->s){
		tmp = q->h;
		q->h = q->h->next;
		if(NULL == q->h){
			tl_assert(1 == q->size);
			tl_assert(q->t == tmp);
			q->t = NULL;
		}
		tmp->next = q->f;
		q->f = tmp;
	}
	q->size --;
}

size_t tl_queue_size(tl_queue_t *q){
	return q->size;
}

tl_queue_ele_t tl_queue_top(tl_queue_t *q){
	if(0 == q->size) { tl_log_here(); return NULL; }
	return (tl_queue_ele_t)((uintptr_t)q->h->container + q->eleSize * (q->h->s));
}

void tl_queue_destroy(tl_queue_t *q){
	freeList(q->f);
	freeList(q->h);
	free(q);
}
