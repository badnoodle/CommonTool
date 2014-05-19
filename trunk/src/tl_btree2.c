
#include <string.h>
#include <stdlib.h>

#include "tl_profile.h"
#include "tl_pool.h"
#include "tl_btree2.h"
#include "tl_alg.h"
#include "tl_queue.h"

#define bt_keycmp	(root->bpc.keycmp)
#define bt_fork		(root->bpc.m_fork)
#define bt_threshold	(bt_split)
#define bt_maxc		(bt_fork + 1)
#define bt_minc		((1 == (1 & bt_maxc)) ? (1 + (bt_maxc >> 1)) : (bt_maxc >> 1))

#define used(node)	((node)->cnt_used)
#define getkv(node, idx)		((node)->pkv[idx])
#define setkv(node, idx, kv)	do { (node)->pkv[idx] = (kv); } while(0)
#define getchild(node, idx)		((node)->pc[idx])
#define setchild(node, idx, c)	do { (node)->pc[idx] = (c); if(NULL != c) { c->parent = node; } } while(0)

#define full(root, node)		(((root)->bpc.m_fork) == used(node))

typedef struct tl_btree_node_t{
	int cnt_used;	/* means the count of key */
	tl_keyval_t *pkv;	/* [1 ~ fork - 1] */
	struct tl_btree_node_t **pc;	/* [1 ~ fork] */
	struct tl_btree_node_t *parent;
}tl_btree_node_t;

struct tl_btree_root_t{
	tl_btree_config_t bpc;
	tl_pool_t *tp;
	tl_btree_node_t *first;
	size_t cnt_key;
};

struct tl_btree_entry_t {
	int idx_key;
	tl_btree_node_t *ploc;
};

enum {R_LOCATE = 0, R_UNEXIST, };

typedef struct {
	int type;
	int idx_key;
	tl_btree_node_t *ploc;
}locate_t;

typedef struct {
	tl_keyval_t kv;
	tl_btree_node_t *pl;
	tl_btree_node_t *pr;
}insert_t;

void tl_btree_showkey(tl_btree_node_t *node);

size_t tl_btree_entrySize() { return sizeof(tl_btree_entry_t); }

static tl_btree_node_t *tb_alloc_node(tl_btree_root_t *root){

#if !(TL_USE_POOL)
	tl_btree_node_t *pnew = (tl_btree_node_t *)calloc(1, sizeof(tl_btree_node_t));

	if(NULL == pnew) { return NULL; }
	pnew->pkv = (tl_keyval_t *)calloc(root->bpc.m_fork, sizeof(tl_keyval_t));
	pnew->pc = (tl_btree_node_t **)calloc(root->bpc.m_fork + 1, sizeof(tl_btree_node_t *));

#else

	tl_btree_node_t *pnew = (tl_btree_node_t *)tl_pool_calloc(root->tp, 1, sizeof(tl_btree_node_t));

	if(NULL == pnew) { return NULL; }
	pnew->pkv = (tl_keyval_t *)tl_pool_calloc(root->tp, root->bpc.m_fork, sizeof(tl_keyval_t));
	pnew->pc = (tl_btree_node_t **)tl_pool_calloc(root->tp, root->bpc.m_fork + 1, sizeof(tl_btree_node_t *));

#endif

	pnew->cnt_used = 0;
	pnew->parent = NULL;

	if(NULL == pnew->pkv) { goto L__failed; }
	if(NULL == pnew->pc) { goto L__failed; }

	return pnew;

L__failed:

#if !(TL_USE_POOL)

	{
		if(NULL != pnew->pkv) { free(pnew->pkv); }
		if(NULL != pnew->pc) { free(pnew->pc); }
		free(pnew);
		return NULL;
	}

#else

	{
		if(NULL != pnew->pkv) { tl_pool_free(root->tp, pnew->pkv); }
		if(NULL != pnew->pc) { tl_pool_free(root->tp, pnew->pc); }
		tl_pool_free(root->tp, pnew);
		return NULL;
	}

#endif
}

#if !(TL_USE_POOL)

static void tb_free_node(tl_btree_root_t *root, tl_btree_node_t *node){
	if(NULL != root) {}
	free(node->pkv);
	free(node->pc);
	free(node);
}

#else

static void tb_free_node(tl_btree_root_t *root, tl_btree_node_t *node){
	tl_pool_free(root->tp, node->pkv);
	tl_pool_free(root->tp, node->pc);
	tl_pool_free(root->tp, node);
}

#endif

static locate_t tb_locate_key(tl_btree_root_t *root, tl_key_t key){
	int idx = 0;

	tl_btree_node_t *pcur = root->first;
	tl_btree_node_t *parent = NULL;
	bool b_find = false;

	while(NULL != pcur && !b_find){

		idx = tl_bsearch(&(sort_config_t){pcur->pkv, sizeof(tl_keyval_t), used(pcur), bt_keycmp}, &key, true);	/* kv[idx] <= key */

		tl_assert(0 <= idx);

		if(used(pcur) > idx && 0 == bt_keycmp(&key, &getkv(pcur, idx).key)){
			b_find = true;
		}else{
			parent = pcur;
			pcur = getchild(pcur, idx);
		}
	}
	if(b_find) { return (locate_t){R_LOCATE, idx, pcur}; }
	else { return (locate_t){R_UNEXIST, idx, parent}; }
}

static int tb_locate_node(tl_btree_node_t *node){
	int i;
	if(NULL == node->parent) { return -1; }
	for(i = 0; i <= used(node->parent); i ++){
		if(getchild(node->parent, i) == node){
			return i;
		}
	}
	tl_assert(false);
	return -1;
}

static void tb_shift_left(tl_btree_node_t *node, int idx, int cnt){
	int i;

	if(idx >= used(node)) { return; }

	for(i = idx; i < used(node); i ++){
		setkv(node, i - cnt, getkv(node, i));
		setchild(node, i - cnt, getchild(node, i));
	}
	setchild(node, i - cnt, getchild(node, i));
}

static void tb_shift_right(tl_btree_node_t *node, int idx, int cnt, bool b_onlykv){
	int i;

	if(idx >= used(node)) { return; }

	for(i = used(node) - 1; i >= idx; i --){
		setkv(node, i + cnt, getkv(node, i));
		if(!b_onlykv){
			setchild(node, i + cnt + 1, getchild(node, i + 1));
		}
	}
	setchild(node, i + 1 + cnt, getchild(node, i + 1));
}

static void insert_2(tl_btree_node_t *node, int idx, insert_t ins){

	tb_shift_right(node, idx, 1, false);	
	setkv(node, idx, ins.kv);

	setchild(node, idx, ins.pl);
	setchild(node, idx + 1, ins.pr);
	used(node) ++;
}

static insert_t insert_3(
		tl_btree_root_t *root, 
		tl_btree_node_t *node, 
		int idx, 
		insert_t ins)
{
	int i;
	int idx_split = bt_minc - 1;
	bool b_new = false;
	bool b_ins = true;
	insert_t res;
	tl_btree_node_t *pnew = tb_alloc_node(root);

	if(NULL == pnew) { tl_assert(false); return (insert_t) {{NULL, NULL}, NULL, NULL}; }

	if(idx_split == idx){
		b_ins = false;
		res = (insert_t) {ins.kv, node, pnew};
	}else{
		if(idx > idx_split){
			b_new = true;
			res = (insert_t) {getkv(node, idx_split), node, pnew};
		}else{
			res = (insert_t) {getkv(node, idx_split - 1), node, pnew};
		}
	}

	if(b_ins && b_new){
		idx_split ++;
	}

	if(0 < (used(node) - idx_split)){
		for(i = 0; i < (used(node) - idx_split); i ++){
			used(pnew) ++;
			setkv(pnew, i, getkv(node, idx_split + i));
			setchild(pnew, i, getchild(node, idx_split + i));
		}
		setchild(pnew, i, getchild(node, idx_split + i));
	}

	if(b_ins){
		if(!b_new){
			used(node) =  bt_minc - 2;
		}else{
			used(node) = bt_minc - 1;
		}
	}else{
		used(node) = idx_split;
	}

	if(b_ins){
		if(b_new){
			idx -= idx_split;
			insert_2(pnew, idx, ins);
		}else{
			insert_2(node, idx, ins);
		}
	}else{
		setchild(node, used(node), ins.pl);
		setchild(pnew, 0, ins.pr);
	}
	pnew->parent = node->parent;
	return res;
}

static int insert_1(tl_btree_root_t *root, tl_btree_node_t *node, int idx, tl_keyval_t *pkv){
	bool b_end = false;
	insert_t ins = {*pkv, NULL, NULL};

	while(NULL != node && !b_end){
		if(full(root, node)){
			ins = insert_3(root, node, idx, ins);
			node = node->parent;
			if(NULL != node){
				idx = tl_bsearch(&(sort_config_t){node->pkv, sizeof(tl_keyval_t), used(node), bt_keycmp}, &ins.kv.key, true);
			}
		}else{
			insert_2(node, idx, ins);
			b_end = true;
		}
	}

	if(!b_end){
		root->first = tb_alloc_node(root);
		/*
		 *	FIXME
		 *		how deal failed ...
		 * */
		if(NULL == root->first) { return -1; }

		setkv(root->first, 0, ins.kv);
		setchild(root->first, 0, ins.pl);
		setchild(root->first, 1, ins.pr);
		used(root->first) ++;
	}

	root->cnt_key ++;
	return 0;
}

static void erase_3(tl_btree_root_t *root, tl_btree_node_t *node, tl_btree_node_t *pbro, bool b_left)
{	
	int idx;

	if(b_left){
		idx = tl_bsearch(&(sort_config_t){node->parent->pkv, sizeof(tl_keyval_t), used(node->parent), bt_keycmp}, &getkv(pbro, used(pbro) - 1).key, true);

		tl_assert(used(node->parent) != idx);

		tb_shift_right(node, 0, 1, false);
		if(0 == used(node)){
			setchild(node, 1, getchild(node, 0));
		}
		setkv(node, 0, getkv(node->parent, idx));
		setchild(node, 0, getchild(pbro, used(pbro)));
		used(node) ++;

		setkv(node->parent, idx, getkv(pbro, used(pbro) - 1));
		used(pbro) --;
	}else{
		idx = tl_bsearch(&(sort_config_t){node->parent->pkv, sizeof(tl_keyval_t), used(node->parent), bt_keycmp}, &getkv(pbro, 0).key, true);

		tl_assert(0 < idx);

		idx --;

		setkv(node, used(node), getkv(node->parent, idx));
		used(node) ++;
		setchild(node, used(node), getchild(pbro, 0));

		setkv(node->parent, idx, getkv(pbro, 0));

		tb_shift_left(pbro, 1, 1);
		used(pbro) --;
	}
}

static void erase_4(tl_btree_root_t *root, tl_btree_node_t *node, tl_btree_node_t *pbro, int idx, bool b_left)
{	
	int i;
	if(b_left){
		tl_assert(pbro == getchild(node->parent, idx - 1));
		//setkv(node, used(node), getkv(node->parent, idx - 1));
		//used(node) ++;

		setkv(pbro, used(pbro), getkv(node->parent, idx - 1));
		used(pbro) ++;

		for(i = 0; i < used(node); i ++){
			setkv(pbro, used(pbro), getkv(node, i));
			setchild(pbro, used(pbro), getchild(node, i));
			used(pbro) ++;
		}
		if(0 < used(node)){
			setchild(pbro, used(pbro), getchild(node, i));
		}
		if(0 == used(node)){
			setchild(pbro, used(pbro), getchild(node, 0));
		}

		tb_shift_left(pbro->parent, idx, 1);
		setchild(pbro->parent, idx - 1, pbro);
	}else{
		setkv(node, used(node), getkv(node->parent, idx));
		used(node) ++;
		tb_shift_right(pbro, 0, used(node), false);
		for(i = 0; i < used(node); i ++){
			setkv(pbro, i, getkv(node, i));
			setchild(pbro, i, getchild(node, i));
			used(pbro) ++;
		}
		tb_shift_left(pbro->parent, idx + 1, 1);
		setchild(pbro->parent, idx, pbro);
	}
	tb_free_node(root, node);
	used(pbro->parent) --;
}

static void erase_2(tl_btree_root_t *root, tl_btree_node_t *node){
	tl_btree_node_t *pl = NULL;
	tl_btree_node_t *pr = NULL;
	int idx = -1;
	int mink = bt_minc - 1;
	bool b_first = true;

	while(true){

		if(mink <= used(node)) { break; }

		idx = tb_locate_node(node);
		if(-1 == idx) {
			tl_assert(root->first == node);
			if(0 == used(node)) { 
				if(b_first){
					root->first = NULL; 
				}else{
					root->first = getchild(node, 0);
					root->first->parent = NULL;
				}
				tb_free_node(root, node);
			}
			break;
		}

		pl = (0 == idx ? NULL : getchild(node->parent, idx - 1));
		pr = (used(node->parent) == idx ? NULL : getchild(node->parent, idx + 1));

		tl_assert((mink - 1) == used(node));

		if(NULL != pr && (mink < used(pr))){
			erase_3(root, node, pr, false);
			break;
		}else if(NULL != pl && (mink < used(pl))){
			erase_3(root, node, pl, true);
			break;
		}

		if(NULL != pr && (mink == used(pr))){
			erase_4(root, node, pr, idx, false);
			node = pr->parent;
		}else if(NULL != pl && (mink == used(pl))){
			erase_4(root, node, pl, idx, true);
			node = pl->parent;
		}else{
			tl_assert(false);
		}
		b_first = false;
	}
}

static void erase_1(tl_btree_root_t *root, tl_btree_node_t *node, int idx){
	tl_btree_node_t *suc = getchild(node, idx + 1);
	if(NULL != suc){
		while(true){
			if(NULL == getchild(suc, 0)){
				break;
			}
			suc = getchild(suc, 0);
		}
	}else{
		suc = node;
	}

	if(suc != node){
		setkv(node, idx, getkv(suc, 0));
		tb_shift_left(suc, 1, 1);
	}else{
		tb_shift_left(node, idx + 1, 1);
	}

	used(suc) --;
	erase_2(root, suc);

	root->cnt_key --;
}

int tl_btree_erase(tl_btree_root_t *root, tl_keyval_t *pkv){
	locate_t loc = tb_locate_key(root, pkv->key);

	if(R_LOCATE == loc.type){
		pkv->val = getkv(loc.ploc, loc.idx_key).val;
		erase_1(root, loc.ploc, loc.idx_key);
		return 0;
	}else{
		return -1;
	}
}

int tl_btree_query(tl_btree_root_t *root, tl_keyval_t *pkv){
	locate_t loc;
	loc = tb_locate_key(root, pkv->key);
	if(R_LOCATE == loc.type) { pkv->val = getkv(loc.ploc, loc.idx_key).val; return 0; }
	else { return -1; }
}

int tl_btree_insert(tl_btree_root_t *root, tl_keyval_t *pkv, bool b_update){
	tl_val_t tmp;
	locate_t loc = tb_locate_key(root, pkv->key);

	if(R_LOCATE == loc.type) {
		if(b_update){
			tmp = getkv(loc.ploc, loc.idx_key).val;
			getkv(loc.ploc, loc.idx_key).val = pkv->val;
			pkv->val = tmp;
			return 0;
		}
		return -1;
	}else{
		return insert_1(root, loc.ploc, loc.idx_key, pkv);
	}
}

tl_btree_root_t *tl_btree_init(const tl_btree_config_t *pbc){

	tl_pool_t *tp = tl_pool_init();
	if(NULL == tp) { return NULL; }

#if !(TL_USE_POOL)
	tl_btree_root_t *root = (tl_btree_root_t *)malloc(sizeof(tl_btree_root_t));
	if(NULL == root) { return NULL; }
#else
	tl_btree_root_t *root = (tl_btree_root_t *)tl_pool_alloc(tp, sizeof(tl_btree_root_t));
	if(NULL == root) {
		tl_pool_destroy(tp);
		return NULL; 
	}

	root->tp = tp;
#endif

	root->bpc = *pbc;
	root->first = NULL;
	root->cnt_key = 0;
	return root;
}

void tl_btree_destroy(tl_btree_root_t *root){
	/*
	 *	FIXME
	 *		done
	 * */
#if !(TL_USE_POOL)
	tl_btree_node_t *node = root->first;
	int i;
	bool b_leaf;

	tl_queue_t *q = tl_queue_init(sizeof(tl_btree_node_t *));

	if(NULL == q) { tl_assert(false); }
	if(NULL != root->first) { tl_queue_push(q, &(root->first)); }

	while(0 < tl_queue_size(q)){
		node = *(tl_btree_node_t **)tl_queue_top(q);	tl_queue_pop(q);
		tl_assert(0 < used(node));
		b_leaf = (NULL == getchild(node, 0));
		for(i = 0; i <= used(node); i ++){
			if(b_leaf) { tl_assert(NULL == getchild(node, i)); }
			else { tl_queue_push(q, &getchild(node, i)); }
		}
		tb_free_node(root, node);
	}

	tl_queue_destroy(q);

	free(root);

#else
	tl_pool_destroy(root->tp);
#endif
}

size_t tl_btree_getcnt(tl_btree_root_t *root){
	return root->cnt_key;
}

int tl_btree_first(tl_btree_root_t *root, tl_btree_entry_t *entry){
	tl_btree_node_t *node = root->first;
	if(NULL != node){
		while(NULL != getchild(node, 0)){
			node = getchild(node, 0);
		}
		entry->idx_key = 0;
		entry->ploc = node;
		return 0;
	}
	return -1;
}

int tl_btree_last(tl_btree_root_t *root, tl_btree_entry_t *entry){
	tl_btree_node_t *node = root->first;
	if(NULL != node){
		while(NULL != getchild(node, used(node))){
			node = getchild(node, used(node));
		}
		entry->idx_key = used(node) - 1;
		entry->ploc = node;
		return 0;
	}
	return -1;
}

int tl_btree_next(tl_btree_entry_t *entry){
	tl_btree_node_t *node = entry->ploc;
	int idx_pc = -1;

	entry->idx_key ++;
	if(NULL != getchild(node, entry->idx_key)){
		node = getchild(node, entry->idx_key);
		while(NULL != getchild(node, 0)){
			node = getchild(node, 0);
		}
		entry->ploc = node;
		entry->idx_key = 0;
		return 0;
	}

	if((used(node) - 1) >= entry->idx_key){
		return 0;
	}else{
		idx_pc = tb_locate_node(node);
		while(-1 != idx_pc && idx_pc == used(node->parent)){
			node = node->parent;
			idx_pc = tb_locate_node(node);
		}

		if(-1 == idx_pc){
			return -1;
		}else{
			entry->idx_key = idx_pc;
			entry->ploc = node->parent;
			return 0;
		}
	}
}

int tl_btree_prev(tl_btree_entry_t *entry){
	tl_btree_node_t *node = entry->ploc;
	int idx_pc = -1;

	if(NULL != getchild(node, entry->idx_key)){
		node = getchild(node, entry->idx_key);

		while(NULL != getchild(node, used(node))){
			node = getchild(node, used(node));
		}

		entry->ploc = node;
		entry->idx_key = used(node) - 1;

		return 0;
	}

	entry->idx_key --;

	if(0 <= entry->idx_key){
		return 0;
	}else{

		idx_pc = tb_locate_node(node);

		if(0 == idx_pc){
			do{
				node = node->parent;
				idx_pc = tb_locate_node(node);
			}while(0 == idx_pc);
		}

		if(-1 == idx_pc){
			return -1;
		}
		node = node->parent;

		idx_pc --;

		entry->idx_key = idx_pc;
		entry->ploc = node;

		return 0;
	}
	return -1;
}

void tl_btree_extract(tl_btree_entry_t *entry, tl_keyval_t *pkv){
	*pkv = getkv(entry->ploc, entry->idx_key);
}

void tl_btree_set(tl_btree_entry_t *entry, tl_val_t val){
	getkv(entry->ploc, entry->idx_key).val = val;
}

int tl_btree_check(tl_btree_root_t *root){
	if(NULL == root->first) { return 0; }

	tl_queue_t *qn = tl_queue_init(sizeof(intptr_t));
	tl_queue_t *qc = tl_queue_init(sizeof(int));
	if(NULL == qn || NULL == qc) { return 0; }

	int cnt, nextc;
	int i;
	int mink = bt_minc - 1;
	tl_btree_node_t *pcur = NULL;

	tl_queue_push(qc, (cnt = 1, &cnt));
	tl_queue_push(qn, &root->first);

	if(NULL != root->first->parent) { return -4; }

	while(0 < tl_queue_size(qc)){
		cnt = *(int *)tl_queue_top(qc);	tl_queue_pop(qc);
		nextc = 0;
		for(i = 0; i < cnt; i ++){
			pcur = *(tl_btree_node_t **)tl_queue_top(qn);
			tl_queue_pop(qn);

			if(NULL != pcur){
				if(root->first != pcur && used(pcur) < mink) { return -1; }
				if(used(pcur) > bt_fork) { return -2; }
				nextc += (1 + used(pcur));
				int j;
				for(j = 0; j <= used(pcur); j ++){
					tl_queue_push(qn, &getchild(pcur, j));
					if(NULL != getchild(pcur, j)){
						if(getchild(pcur, j)->parent != pcur){
							return -3;
						}
					}
				}
			}
		}
		if(0 < nextc) { tl_queue_push(qc, &nextc); }
	}

	tl_queue_destroy(qc);
	tl_queue_destroy(qn);
	return 0;
}

void tl_btree_showkey(tl_btree_node_t *node){
	if(NULL == node) { tl_trace( "[]"); return; }
	int i;
	tl_trace( "[");
	for(i = 0; i < used(node); i ++){
		tl_trace( "%p ", (void *)getkv(node, i).key);
	}
	tl_trace( "]");
}

void tl_btree_show(tl_btree_root_t *root){
	tl_queue_t *qn = tl_queue_init(sizeof(intptr_t));
	tl_queue_t *qc = tl_queue_init(sizeof(int));
	if(NULL == qn || NULL == qc) { return; }
	if(NULL == root->first) { return; }

	int cnt, nextc;
	int i;
	tl_btree_node_t *pcur = NULL;

	tl_queue_push(qc, (cnt = 1, &cnt));
	tl_queue_push(qn, &root->first);

	while(0 < tl_queue_size(qc)){
		cnt = *(int *)tl_queue_top(qc);	tl_queue_pop(qc);
		nextc = 0;
		for(i = 0; i < cnt; i ++){
			pcur = *(tl_btree_node_t **)tl_queue_top(qn);
			tl_queue_pop(qn);

			if(NULL != pcur){
				nextc += (1 + used(pcur));
				int j;
				for(j = 0; j <= used(pcur); j ++){
					tl_queue_push(qn, &getchild(pcur, j));
					if(NULL != getchild(pcur, j)){
						tl_assert(getchild(pcur, j)->parent == pcur);
					}
				}
			}
			tl_btree_showkey(pcur);
		}
		tl_trace( "\n");
		if(0 < nextc) { tl_queue_push(qc, &nextc); }
	}

	tl_queue_destroy(qc);
	tl_queue_destroy(qn);
}
