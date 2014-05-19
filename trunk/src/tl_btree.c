
#include <stdlib.h>
#include <string.h>

#include "tl_assert.h"

#include "tl_pool.h"
#include "tl_btree.h"

#define MASK			(0X80000000)
#define BT_MAX_FORK	 256

#define bt_firstLeaf(root)	  ((root)->firstLeaf)
#define bt_firstNode(root)	  ((root)->firstNode)	  
#define bt_fork(root)		   ((root)->bt_config.m_fork)
#define bt_split(root)		  ((bt_fork(root) + 1) >> 1)
#define bt_mergeThreshold(root) (bt_fork(root) - bt_split(root))
#define bt_keycmp(root)		 ((root)->bt_config.keycmp)
#define bt_legalKey(root)	((root)->bt_config.legalKey)
#define bt_legalVal(root)	((root)->bt_config.legalVal)

#define leaf(node)					(0 > (node)->cnt_used)
#define init(node, cnt) \
	do { \
		if(leaf(node)) { (node)->cnt_used = (cnt) | MASK; } \
		else {(node)->cnt_used = (cnt); } \
	}while(0)

#define used(node)					((int)((node)->cnt_used & (~ MASK)))
#define inc(node, val)				do { (node)->cnt_used += (val); } while(0)
#define dec(node, val)				do { (node)->cnt_used -= (val); } while(0)

#define isleaf(root, deep)			((root)->cnt_deep == (deep))
#define full(root, node)			  (bt_fork(root) == used(node))
#define getkey(node, idx)			  ((node)->key[idx])
#define getmaxkey(node)			  (getkey((node), used(node) - 1))
#define setkey(node, idx, k)		  ((node)->key[idx] = (k))
#define getchild(node, idx)		   ((node)->child[idx])
#define setchild(node, idx, c)	((node)->child[idx] = (c))
#define setval(leaf, idx, val)		((leaf)->pval[idx] = (val))
#define getval(leaf, idx)			 ((leaf)->pval[idx])
#define extra(root, node)			 ((used(node) - bt_mergeThreshold(root)) >> 1)

#if !(TL_USE_POOL)

#define realFree(pfree /*although used for leaf*/)	do { \
	if(NULL != pfree->key) { free(pfree->key); }\
	if(NULL != pfree->child) { free(pfree->child); }\
	free(pfree);\
} while(0)

#else

#define realFree(pfree /*although used for leaf*/)	do { \
	if(NULL != pfree->key) { tl_pool_free(root->tp, pfree->key); }\
	if(NULL != pfree->child) { tl_pool_free(root->tp, pfree->child); }\
	tl_pool_free(root->tp, pfree);\
} while(0)

#endif


struct tl_bplustree_leaf_t;

typedef struct tl_bplustree_node_t{
	intptr_t cnt_used;
	tl_key_t *key;

	union{
		struct tl_bplustree_node_t *parent;
		struct tl_bplustree_node_t *next;
	};

	union{
		struct tl_bplustree_node_t **child;
		tl_val_t *pval;
	};
}tl_bplustree_node_t;

struct tl_bplustree_root_t{

	tl_pool_t *tp;
	tl_bplustree_config_t bt_config;
	struct tl_bplustree_leaf_t *firstLeaf;
	struct tl_bplustree_node_t *firstNode;

	struct tl_bplustree_leaf_t *freeLeaf;
	struct tl_bplustree_node_t *freeNode;

	intptr_t cnt_deep;
	uintptr_t cnt_key;
};

typedef struct tl_bplustree_leaf_t{
	intptr_t cnt_used;
	tl_key_t *key;

	union {
		struct tl_bplustree_node_t *parent;
		struct tl_bplustree_leaf_t *next;
	};

	union{
		struct tl_bplustree_node_t **child;
		tl_val_t *pval;
	};
	struct tl_bplustree_leaf_t *nextLeaf;
}tl_bplustree_leaf_t;

enum {R_LOCATE = 0, R_NOINLEAF, };
enum {I_FAILED = 0, I_SPLITUPDATE = 1, I_SPLIT, I_OKUPDATE, I_OK, };

typedef struct {
	int type;
	int idx_key;
	int idx_dad;
	tl_bplustree_node_t *ploc;
	int cur_deep;
}locate_t;

typedef struct {
	int type;
	tl_bplustree_node_t *pbro;
}insert_t;

#define locate_init { -1, -1, -1, NULL, -1 }
#define insert_init { -1, NULL }

uintptr_t tl_bplustree_getcnt(const tl_bplustree_root_t *root){
	return root->cnt_key;
}

static void tb_free_node(tl_bplustree_root_t * root, tl_bplustree_node_t *pfree){
	/*
	 *	FIXME
	 *		< done >
	 * */
	tl_assert(pfree);
	pfree->next = root->freeNode;
	root->freeNode = pfree;

	pfree->cnt_used = 0;
}

static void tb_free_leaf(tl_bplustree_root_t * root, tl_bplustree_leaf_t *pfree){
	/*
	 *	FIXME
	 *		< done >
	 * */
	tl_assert(pfree);
	pfree->next = root->freeLeaf;
	root->freeLeaf = pfree;
	pfree->cnt_used = 0;
}

static tl_bplustree_node_t *tb_alloc_node(tl_bplustree_root_t *root){
	/*
	 *	FIXME
	 *		< done > ...
	 * */
	tl_bplustree_node_t *node = NULL;

	if(NULL == root->freeNode){

#if !(TL_USE_POOL)
		node = (tl_bplustree_node_t *)malloc(sizeof(tl_bplustree_node_t));
#else
		node = (tl_bplustree_node_t *)tl_pool_calloc(root->tp, 1, sizeof(tl_bplustree_node_t));
#endif

		if(NULL == node) { return NULL; }

#if !(TL_USE_POOL)
		memset(node, 0, sizeof(tl_bplustree_node_t));
#endif

#if !(TL_USE_POOL)
		node->key = (tl_key_t *)malloc(bt_fork(root) * sizeof(tl_key_t));
#else
		node->key = (tl_key_t *)tl_pool_alloc(root->tp, bt_fork(root) * sizeof(tl_key_t));
#endif

		if(NULL == node->key) {
			realFree(node);
			return NULL;
		}

#if !(TL_USE_POOL)
		node->child = (tl_bplustree_node_t **)malloc(bt_fork(root) * sizeof(tl_bplustree_node_t));
#else
		node->child = (tl_bplustree_node_t **)tl_pool_alloc(root->tp, 
				bt_fork(root) * sizeof(tl_bplustree_node_t));
#endif

		if(NULL == node->child) {
			realFree(node);
			return NULL; 
		}
	}else{
		node = root->freeNode;
		root->freeNode = root->freeNode->next;
	}

	memset(node->key, 0, sizeof(tl_key_t ) * bt_fork(root));
	memset(node->child, 0, sizeof(tl_bplustree_node_t *) * bt_fork(root));
	node->parent = NULL;

	return node;
}

static tl_bplustree_leaf_t *tb_alloc_leaf(tl_bplustree_root_t *root){
	/*
	 *	FIXME
	 *		< done >
	 * */
	tl_bplustree_leaf_t *leaf = NULL;

	if(NULL == root->freeLeaf){

#if !(TL_USE_POOL)
		leaf = (tl_bplustree_leaf_t *)malloc(sizeof(tl_bplustree_leaf_t));
#else
		leaf = (tl_bplustree_leaf_t *)tl_pool_alloc(root->tp, sizeof(tl_bplustree_leaf_t));
#endif

		if(NULL == leaf) { return NULL; }
		memset(leaf, 0, sizeof(tl_bplustree_leaf_t));

#if !(TL_USE_POOL)
		leaf->key = (tl_key_t *)malloc(bt_fork(root) * sizeof(tl_key_t));
#else
		leaf->key = (tl_key_t *)tl_pool_alloc(root->tp, bt_fork(root) * sizeof(tl_key_t));
#endif

		if(NULL == leaf->key) { 
			realFree(leaf);
			return NULL; 
		}

#if !(TL_USE_POOL)
		leaf->pval = (tl_val_t *)malloc(bt_fork(root) * sizeof(tl_val_t));
#else
		leaf->pval = (tl_val_t *)tl_pool_alloc(root->tp, bt_fork(root) * sizeof(tl_val_t));
#endif

		if(NULL == leaf->pval) {
			realFree(leaf);
			return NULL; 
		}
	}else{
		leaf = root->freeLeaf;
		root->freeLeaf = root->freeLeaf->next;
	}

	memset(leaf->key, 0, sizeof(tl_key_t ) * bt_fork(root));
	memset(leaf->child, 0, sizeof(tl_bplustree_leaf_t *) * bt_fork(root));
	leaf->parent = NULL;

	leaf->nextLeaf = NULL;
	leaf->cnt_used |= MASK;

	return leaf;
}

static int tb_locate_key(const tl_bplustree_root_t *root, const tl_bplustree_node_t *node, tl_key_t key){
	int idx = 0;
	for(idx = 0; idx < used(node); idx ++){
		if(-1 < bt_keycmp(root)(getkey(node, idx), key)){
			break;
		}
	}
	return idx;
}

static void tb_shift_right(tl_bplustree_node_t *node, int idx, int cnt){
	int i;
	for(i = used(node) - 1; i >= idx; i --){
		setkey(node, i + cnt, getkey(node, i));
		setchild(node, i + cnt, getchild(node, i));
	}
}

static void shiftLeft(tl_bplustree_node_t *node, int idx, int cnt){

	int i;
	tl_assert(0 <= idx - cnt);

	for(i = idx; i < used(node); i ++){
		setkey(node, i - cnt, getkey(node, i));
		setchild(node, i - cnt, getchild(node, i));
	}
}

static int updateUp(tl_bplustree_root_t *root, tl_bplustree_node_t *pcur, int idx_dad){

	bool b_check = false;
	tl_key_t pcurKey = NULL;

	if(NULL != pcur->parent){
		tl_assert(pcur == getchild((pcur->parent), idx_dad));
		if(!b_check || -1 == bt_keycmp(root)(getmaxkey(pcur), getkey((pcur->parent), idx_dad))){
			while(NULL != pcur->parent){
				pcurKey = getkey((pcur->parent), idx_dad);
				setkey((pcur->parent), idx_dad, getmaxkey(pcur));
				pcur = pcur->parent;
				if(idx_dad < (used(pcur) - 1)){
					break;
				}else if(NULL != pcur->parent){
					idx_dad = tb_locate_key(root, pcur->parent, pcurKey);
					tl_assert(0 >= bt_keycmp(root)(pcurKey, getkey((pcur->parent), idx_dad)));
				}else{
					break;
				}
			}
		}else if(!b_check){
			tl_assert(0 == bt_keycmp(root)(getmaxkey(pcur), getkey((pcur->parent), idx_dad)));
		}
	}
	return 0;
}

static void tb_insert_node(tl_bplustree_node_t *node,
		int idx,
		tl_key_t key, 
		tl_bplustree_node_t *pchild)
{
	tb_shift_right(node, idx, 1);
	setkey(node, idx, key);
	setchild(node, idx, pchild);

	if(!leaf(node)){
		pchild->parent = node;
	}

	inc(node, 1);
}

static void tb_insert_leaf(tl_bplustree_leaf_t *leaf,
		int idx,
		tl_key_t *key,
		tl_val_t *val)
{
	tb_shift_right((tl_bplustree_node_t *)leaf, idx, 1);
	setkey(leaf, idx, key);
	setval(leaf, idx, val);
	inc(leaf, 1);
}

static int rebalance(const tl_bplustree_root_t *root, tl_bplustree_node_t *node, int idx){

	tl_bplustree_node_t *pl = (0 == idx ? NULL : getchild(node, idx - 1));
	tl_bplustree_node_t *pr = ((used(node) - 1) == idx ? NULL : getchild(node, idx + 1));
	tl_bplustree_node_t *pcur = getchild(node, idx);

	int i;
	int cnt_l = (NULL == pl ? -1 : used(pl));
	int cnt_r = (NULL == pr ? -1 : used(pr));

	tl_bplustree_node_t *pfrom = (cnt_l >= cnt_r) ? pl : pr;
	if(NULL == pfrom) { return -1; }
	int extra = extra(root, pfrom);
	if(0 >= extra) { return -1; }

	if(pl == pfrom){
		for(i = 0; i < extra; i ++){
			tb_insert_node(pcur, i, 
					getkey(pfrom, used(pfrom) - extra + i), 
					getchild(pfrom, used(pfrom) - extra + i));
		}
		dec(pfrom, extra);
		setkey(node, idx - 1, getmaxkey(pfrom));
	}else{
		for(i = 0; i < extra; i ++){
			tb_insert_node(pcur, used(pcur), 
					getkey(pfrom, i), 
					getchild(pfrom, i));
		}
		shiftLeft(pfrom, extra, extra);
		dec(pfrom, extra);
		setkey(node, idx, getmaxkey(pcur));
	}
	return 0;
}

/*
 * max key in dst < min key in src
 * that is to say, the src is in right of dst
 * */
static void merge2(const tl_bplustree_root_t *root, tl_bplustree_node_t *dst, tl_bplustree_node_t *src){

	tl_assert(bt_fork(root) >= (used(dst) + used(src)));

	if(root) {}

	int idx;
	for(idx = 0; idx < used(src); idx ++){
		tb_insert_node(dst, used(dst), getkey(src, idx), getchild(src, idx));
	}
	init(src, 0);
}

static int merge(tl_bplustree_root_t *root, tl_bplustree_node_t *node, int idx/*, bool &b_link*/){
	tl_bplustree_node_t *pl = (0 == idx ? NULL : getchild(node, idx - 1));
	tl_bplustree_node_t *pr = (((used(node) - 1) <= idx) ? NULL : getchild(node, idx + 1));

	tl_bplustree_node_t *pcur = getchild(node, idx);
	int idx_del;

	bool b_reset = true;
	bool b_link = false;

	if(NULL != pl && bt_fork(root) >= (used(pl) + used(pcur))){
		merge2(root, pl, pcur);
		idx_del = idx;
		if(leaf(pl)){
			((tl_bplustree_leaf_t *)pl)->nextLeaf = ((tl_bplustree_leaf_t *)pcur)->nextLeaf;
		}
	}else if(NULL != pr && bt_fork(root) >= (used(pr) + used(pcur))){
		merge2(root, pcur, pr);
		idx_del = idx + 1;
		if(leaf(pcur)){
			((tl_bplustree_leaf_t *)pcur)->nextLeaf = ((tl_bplustree_leaf_t *)pr)->nextLeaf;
		}
	}else if(NULL != pl || NULL != pr){
		return -1;
	}else{
		tl_assert(1 == used(node));
		idx_del = 0;
		b_reset = false;

		if(leaf((getchild(node, 0)))){
			if((tl_bplustree_node_t *)root->firstLeaf == getchild(node, 0)){
				tl_bplustree_leaf_t **p = (tl_bplustree_leaf_t **)&(root->firstLeaf);
				*p = ((tl_bplustree_leaf_t *)getchild(node, 0))->nextLeaf;
			}else{
				b_link = true;
			}
		}
	}

	if(b_reset){
		setkey(node, idx_del - 1, getkey(node, idx_del));
	}
	/*
	 *	MODIFY TAG HERE
	 * */
	if(leaf(getchild(node, idx_del))){
		tb_free_leaf(root, (tl_bplustree_leaf_t *)getchild(node, idx_del));
	}else{
		tb_free_node(root, getchild(node, idx_del));
	}

	shiftLeft(node, idx_del + 1, 1);
	dec(node, 1);

	return 0;
}

static int tb_do_del(tl_bplustree_root_t *root, locate_t loc, tl_key_t key){

	tl_bplustree_node_t *pcur = loc.ploc;
	tl_bplustree_node_t *parent = NULL;
	int idx = loc.idx_key;
	int idx_dad = loc.idx_dad;
	bool b_ok = false;

	tl_assert(leaf(pcur));

	shiftLeft(pcur, idx + 1, 1);
	dec(pcur, 1);

	tl_assert(0 <= used(pcur));

	if(idx == used(pcur)){			
		if(0 < used(pcur)){
			updateUp(root, pcur, idx_dad);
			key = getmaxkey(pcur);
		}
	}

	while(NULL != pcur->parent){

		idx_dad = tb_locate_key(root, pcur->parent, key);

		parent = pcur->parent;
		b_ok = (used(pcur) <= bt_mergeThreshold(root)) && 
			((0 == merge(root, parent, idx_dad)) || 
			 (0 == rebalance(root, parent, idx_dad)));
		pcur = parent;
	}

	if(parent == root->firstNode){
		if(!leaf((root->firstNode)) && 1 == used((root->firstNode))){

			tl_assert(b_ok);

			tl_bplustree_node_t *tmp = root->firstNode;
			tl_bplustree_node_t **p = (tl_bplustree_node_t **)(&(root->firstNode));

			*p = getchild(tmp, 0);
			root->firstNode->parent = NULL;

			(*p)->parent = NULL;

			tb_free_node(root, tmp);

			int *deep = (int *)(&(root->cnt_deep));
			(*deep) --;
		}
	}else if(NULL == parent){
		tl_assert((void *)root->firstNode == (void *)root->firstLeaf);
		if(0 == used(root->firstNode)){
			/*
			 *	MODIFY TAG HERE
			 *		change freeNode -> freeLeaf
			 * */
			tb_free_leaf(root, (tl_bplustree_leaf_t *)root->firstNode);
			root->firstNode = NULL;
			root->firstLeaf = NULL;
		}
	}

	root->cnt_key --;
	return 0;
}

static tl_bplustree_node_t *tb_split_node(tl_bplustree_root_t *root, 
		tl_bplustree_node_t *node, int idx, 
		tl_key_t key, tl_bplustree_node_t *pchild)
{
	tl_bplustree_node_t *newN = tb_alloc_node(root);
	if(NULL == newN) { return NULL; }

	bool b_new = false;
	int idx_add = 0;
	int i = 0;

	idx_add = bt_split(root);

	if(idx < bt_split(root)){
		idx_add --;
	}else{
		idx -= bt_split(root);
		b_new = true;
	}

	for(i = idx_add; i < used(node); i ++){
		tb_insert_node(newN, i - idx_add, 
				getkey(node, i), 
				getchild(node, i));
	}

	{
		init(node, bt_split(root));
		if(!b_new){
			dec(node, 1);
		}
	}

	if(b_new){
		tb_insert_node(newN, idx, key, pchild);
	}else{
		tb_insert_node(node, idx, key, pchild);
	}

	return newN;
}

static tl_bplustree_leaf_t *tb_split_leaf(tl_bplustree_root_t *root, tl_bplustree_leaf_t *leaf, int idx, tl_keyval_t *pkv){

	tl_bplustree_leaf_t *newL = tb_alloc_leaf(root);
	if(NULL == newL) { return NULL; }

	tl_assert(full(root, leaf));

	bool b_new = false;
	int idx_add = 0;
	int i;

	idx_add = bt_split(root);

	if(bt_split(root) > idx){
		idx_add --;
	}else{
		idx -= bt_split(root);
		b_new = true;
	}

	for(i = idx_add; i < used(leaf); i ++){
		tb_insert_leaf(newL, i - idx_add, 
				getkey(leaf, i), 
				getval(leaf, i));
	}

	{
		init(leaf, bt_split(root));

		if(!b_new){
			dec(leaf, 1);
		}
	}

	tl_assert((used(newL) + used(leaf)) == bt_fork(root));

	if(b_new){
		tb_insert_leaf(newL, idx, pkv->key, pkv->val);
	}else{
		tb_insert_leaf(leaf, idx, pkv->key, pkv->val);
	}

	return newL;
}

static insert_t tb_insert_in_node(tl_bplustree_root_t *root, 
		tl_bplustree_node_t *node, 
		int idx, 
		tl_key_t key, 
		tl_bplustree_node_t *pchild)
{
	insert_t ins = insert_init;
	tl_bplustree_node_t *pbro = NULL;
	if(full(root, node)){
		pbro = tb_split_node(root, node, idx, key, pchild);

		if(NULL == pbro) { ins.type = I_FAILED; return ins; }

		if(0 == bt_keycmp(root)(key, getmaxkey(pbro))){
			ins.type = I_SPLITUPDATE;
		}else{
			ins.type = I_SPLIT;
		}

		pbro->parent = node->parent;
		ins.pbro = pbro;

	}else{
		tb_insert_node(node, idx, key, pchild);
		ins.type = ((idx == (used(node) - 1)) ? I_OKUPDATE : I_OK);
		ins.pbro = NULL;
	}
	return ins;
}

static insert_t tb_insert_in_leaf(tl_bplustree_root_t *root, tl_bplustree_leaf_t *leaf, int idx, tl_keyval_t *pkv){

	tl_bplustree_leaf_t *pbro = NULL;
	insert_t res = insert_init;

	if(full(root, leaf)){

		pbro = tb_split_leaf(root, leaf, idx, pkv);

		if(NULL == pbro) { res.type = I_FAILED; return res; }

		pbro->nextLeaf = leaf->nextLeaf;
		leaf->nextLeaf = pbro;

		if(0 == bt_keycmp(root)(pkv->key, getmaxkey(pbro))){
			res.type = I_SPLITUPDATE;
		}else{
			res.type = I_SPLIT;
		}
		pbro->parent = leaf->parent;
		res.pbro = (tl_bplustree_node_t *)pbro;
	}else{
		tb_insert_leaf(leaf, idx, pkv->key, pkv->val);
		res.type = ((idx == (used(leaf) - 1)) ? I_OKUPDATE : I_OK);
		res.pbro = NULL;
	}
	return res;
}

static int tb_do_insert(tl_bplustree_root_t *root, locate_t loc, insert_t insRes){

	tl_bplustree_node_t *pcur = loc.ploc;
	tl_bplustree_node_t *parent = NULL;
	tl_key_t pcurKey = NULL;
	int idx_dad = loc.idx_dad;
	int idx;
	bool b_first = true;

	while(NULL != pcur->parent){

		if(I_OKUPDATE > insRes.type){
			if(!b_first){
				idx_dad = tb_locate_key(root, pcur->parent, getmaxkey((insRes.pbro)));
			}else{
				b_first = false;
			}
			setchild((pcur->parent), idx_dad, insRes.pbro);

			if(I_SPLITUPDATE == insRes.type){
				updateUp(root, insRes.pbro, idx_dad);
			}

			pcurKey = getmaxkey(pcur);
			idx = tb_locate_key(root, pcur->parent, pcurKey);

			parent = pcur->parent;
			insRes = tb_insert_in_node(root, parent, idx, pcurKey, pcur);

			pcur = parent;

			if(I_FAILED == insRes.type){
				return -1;
			}
		}else if(I_OKUPDATE == insRes.type){
			updateUp(root, pcur, idx_dad);
			break;
		}else{
			break;
		}
	}

	if(I_OKUPDATE > insRes.type && NULL == pcur->parent){

		tl_bplustree_node_t *newN = tb_alloc_node(root);
		if(NULL == newN){
			return -1;
		}
		root->firstNode = newN;

		pcur->parent = newN;
		insRes.pbro->parent = newN;

		tb_insert_node(bt_firstNode(root), 0, getmaxkey(pcur), pcur);
		tb_insert_node(bt_firstNode(root), 1, getmaxkey((insRes.pbro)), insRes.pbro);

		root->cnt_deep ++;
	}

	root->cnt_key ++;
	return 0;
}

static locate_t tb_find_leaf(const tl_bplustree_root_t *root, tl_key_t key){

	locate_t loc = locate_init;
	int idx, dad = 0;
	intptr_t cur_deep = 0;
	const tl_bplustree_node_t *pcur = bt_firstNode(root);
	if(NULL == pcur){
		tl_assert(NULL == root->firstLeaf);
		loc.ploc = NULL;
		loc.type = R_NOINLEAF;
		return loc;
	}

	while(true){

		idx = tb_locate_key(root, pcur, key);

		if(isleaf(root, cur_deep)){
			break;
		}else if(idx < used(pcur)){
			pcur = getchild(pcur, idx);
		}else{
			tl_assert(idx == used(pcur));
			idx --;
			pcur = getchild(pcur, idx);
			//break;
		}
		dad = idx;
		cur_deep ++;
	}

	loc.ploc = (tl_bplustree_node_t *)pcur;
	loc.idx_key = idx;
	loc.idx_dad = dad;

	if(isleaf(root, cur_deep) && 
			((0 < used(pcur) && used(pcur) > idx) &&
			 0 == bt_keycmp(root)(getkey(pcur, idx), key)))
	{
		loc.type = R_LOCATE;
	}else if(isleaf(root, cur_deep)){
		loc.type = R_NOINLEAF;
	}else{
		tl_assert(false);
	}
	return loc;
}

tl_bplustree_root_t *tl_bplustree_init(const tl_bplustree_config_t *pconfig){

	tl_bplustree_root_t *root = NULL;
	tl_pool_t *tp = tl_pool_init();

	if(NULL == tp) { return NULL; }
	if(NULL == pconfig || 3 > pconfig->m_fork) { return NULL; }
	if(NULL == pconfig->keycmp) { return NULL; }

#if !(TL_USE_POOL)
	root = (tl_bplustree_root_t *)calloc(1, sizeof(tl_bplustree_root_t));
#else
	root = (tl_bplustree_root_t *)tl_pool_calloc(tp, 1, sizeof(tl_bplustree_root_t));
#endif

	if(NULL == root) {
		tl_pool_destroy(tp);
		return NULL; 
	}

	root->bt_config = *pconfig;
	root->cnt_deep = 0;
	root->tp = tp;

	return root;
}

int tl_bplustree_erase(tl_bplustree_root_t *root, tl_keyval_t *pkey){
	locate_t loc = tb_find_leaf(root, pkey->key);
	if(R_LOCATE != loc.type){
		return -1;
	}else{
		tl_assert(NULL != loc.ploc);
		pkey->val = getval(loc.ploc, loc.idx_key);
		return tb_do_del(root, loc, pkey->key);
	}
}

int tl_bplustree_destroy(tl_bplustree_root_t *root){

#if !(TL_USE_POOL)
	tl_keyval_t kv;
	int cnt = 0;
	while(true){
		kv = tl_bplustree_get(root, 0);
		if(NULL == kv.key){
			break;
		}
		if(-1 == tl_bplustree_erase(root, &kv)){
			tl_assert(false);
		}
		cnt ++;
	}

	tl_bplustree_node_t *tn = NULL;
	tl_bplustree_leaf_t *tl = NULL;

	while(NULL != root->freeNode){
		tn = root->freeNode;
		root->freeNode = root->freeNode->next;
		realFree(tn);
	}

	while(NULL != root->freeLeaf){
		tl = root->freeLeaf;
		root->freeLeaf = root->freeLeaf->next;
		realFree(tl);
	}

	free(root);
	return cnt;

#else
	tl_pool_destroy(root->tp);
	return 0;
#endif
}

int tl_bplustree_query(struct tl_bplustree_root_t *root, tl_keyval_t * pkv){

	locate_t loc = tb_find_leaf(root, pkv->key);

	if(R_LOCATE != loc.type){
		return -1;
	}else{
		pkv->val = getval(loc.ploc, loc.idx_key);
		return 0;
	}
}

tl_keyval_t tl_bplustree_get(const tl_bplustree_root_t *root, int idx){
	const tl_bplustree_leaf_t *pleaf = root->firstLeaf;
	const tl_bplustree_leaf_t *prev = pleaf;
	int cnt_skip = 0;
	tl_keyval_t kv;

	kv.key = NULL;
	kv.val = NULL;

	if(NULL == pleaf){
		return kv;
	}

	if(!(0 < used(pleaf) && bt_fork(root) >= used(pleaf))){
		//ttl_trace( "<%d:%d:%d>\n", used(pleaf), bt_fork(root), root->cnt_deep);
		tl_assert(false);
	}

	cnt_skip = used(pleaf);
	while(cnt_skip < (idx + 1)){
		prev = pleaf;
		pleaf = pleaf->nextLeaf;
		if(NULL != pleaf){
			cnt_skip += used(pleaf);
		}else{
			break;
		}
	}
	if(cnt_skip >= (idx + 1)){
		cnt_skip -= used(pleaf);
		tl_assert(used(pleaf) > (idx - cnt_skip) && 0 <= (idx - cnt_skip));
		kv.key = getkey(pleaf, idx - cnt_skip);
		kv.val = getval(pleaf, idx - cnt_skip);
	}
	return kv;
}

int tl_bplustree_insert(tl_bplustree_root_t *root, tl_keyval_t *pkv, bool b_update){

	tl_bplustree_node_t *pcur = NULL;
	int idx;

	locate_t loc = tb_find_leaf(root, pkv->key);

	if(R_LOCATE == loc.type){
		/*
		 * FIXME
		 *  how to deal exist ...
		 *  < done >
		 * */
		if(b_update){
			setchild(loc.ploc, loc.idx_key, pkv->val);
			return 0;
		}
		return -1;
	}else if(NULL == loc.ploc){
		root->firstLeaf = tb_alloc_leaf(root);
		root->firstNode = (tl_bplustree_node_t *)root->firstLeaf;
		loc.ploc = root->firstNode;
		loc.idx_key = 0;	
	}

	pcur = loc.ploc;
	idx = loc.idx_key;

	insert_t insRes;
	insRes = tb_insert_in_leaf(root, (tl_bplustree_leaf_t *)pcur, idx, pkv);

	if(I_FAILED == insRes.type) { return -1; }
	else {
		return tb_do_insert(root, loc, insRes);
	}
}
