
#include <string.h>
#include <stdlib.h>

#include "tl_assert.h"
#include "tl_pool.h"
#include "tl_rbtree.h"

#define rb_firstNode(root)	((root)->firstN)
#define rb_keycmp(root)		((root)->rbc.keycmp)
#define isforge(node)		(&forgeNode == (node))

enum { RB_RED = 1, RB_BLACK, };
enum { L_UNEXIST = -1, L_LOCATE = 0, L_LCHILD, L_RCHILD, };

struct rb_node_t;
typedef struct rb_node_t rb_node_t;

typedef struct {
    short type;
    rb_node_t *loc;
}locate_t;

struct rb_node_t{
    tl_key_t key;
    tl_val_t val;
    unsigned char color;
    rb_node_t *father;

    rb_node_t *lc;
    rb_node_t *rc;
};

struct tl_rbtree_root_t {
    rb_config_t rbc;
    tl_pool_t *tp;
    rb_node_t *firstN;
    rb_node_t *freeN;

    size_t	cnt_key;
};

struct tl_rbtree_entry_t{
    rb_node_t *ploc;
};

static rb_node_t forgeNode = { NULL, NULL, RB_BLACK, NULL, &forgeNode, &forgeNode };

static inline bool isLeaf(const rb_node_t *pn){
    if(NULL == pn) { tl_assert(false); }
    return pn == &forgeNode;
}

size_t tl_rbtree_entrySize(){
    return sizeof(tl_rbtree_entry_t);
}

static rb_node_t *trb_alloc_node(tl_rbtree_root_t *root, tl_keyval_t *pkv, rb_node_t *father, unsigned char cr){
    rb_node_t *node = NULL;

#if !(TL_USE_POOL)
    node = (rb_node_t *)malloc(sizeof(rb_node_t));
#else
    node = (rb_node_t *)tl_pool_alloc(root->tp, sizeof(rb_node_t));
#endif

    if(NULL == node) { return NULL; }
    node->father = father;
    node->lc = node->rc = &forgeNode;
    node->key = pkv->key;
    node->val = pkv->val;
    node->color = cr;

    root->cnt_key ++;
    return node;
}

static void trb_free_node(tl_rbtree_root_t *root, rb_node_t *node){
#if !(TL_USE_POOL)
    free(node);
#else
    tl_pool_free(root->tp, node);
#endif

    root->cnt_key --;
    return;
}

static inline rb_node_t *brother(rb_node_t *father, rb_node_t *pcur, bool *pleft){
    return (pcur == father->lc) ? (*pleft = true, father->rc) : (*pleft = false, father->lc);
}

static locate_t locateKey(tl_rbtree_root_t *root, tl_key_t key){
    int ret;
    locate_t res;
    rb_node_t *pcur = rb_firstNode(root);
    if(NULL == pcur){
        res.type = L_UNEXIST;
        res.loc = NULL;
        return res;
    }

    while(true){
        ret = rb_keycmp(root)(key, pcur->key);
        if(0 == ret){
            res.type = L_LOCATE;
            res.loc = pcur;
            break;
        }else if(0 > ret){
            if(isforge(pcur->lc)){
                res.type = L_LCHILD;
                res.loc = pcur;
                break;
            }else{
                pcur = pcur->lc;
            }
        }else{
            if(isforge(pcur->rc)){
                res.type = L_RCHILD;
                res.loc = pcur;
                break;
            }else{
                pcur = pcur->rc;
            }
        }
    }
    return res;
}

static void l_rotate(tl_rbtree_root_t *root, rb_node_t *pcur){
    rb_node_t *father = pcur->father;
    rb_node_t *ptr = pcur->rc;

    pcur->rc = ptr->lc;
    ptr->lc->father = pcur;
    ptr->father = father;

    if(NULL == father){
        rb_firstNode(root) = ptr;
    }else{
        if(pcur == father->lc){
            father->lc = ptr;
        }else{
            father->rc = ptr;
        }
    }

    pcur->father = ptr;
    ptr->lc = pcur;
}

static void r_rotate(tl_rbtree_root_t *root, rb_node_t *pcur){
    rb_node_t *father = pcur->father;
    rb_node_t *ptr = pcur->lc;

    pcur->lc = ptr->rc;
    ptr->rc->father = pcur;
    ptr->father = father;

    if(NULL == father){
        tl_assert(rb_firstNode(root) == pcur);
        rb_firstNode(root) = ptr;
        rb_firstNode(root)->father = NULL;
    }else{
        if(pcur == father->lc){
            father->lc = ptr;
        }else{
            tl_assert(pcur == father->rc);
            father->rc = ptr;
        }
    }

    pcur->father = ptr;
    ptr->rc = pcur;
}

static rb_node_t *succeed(rb_node_t *pcur){

    tl_assert(&forgeNode != pcur);

    pcur = pcur->rc;
    if(isforge(pcur)){
        return NULL;
    }

    while(!isforge(pcur->lc)){
        pcur = pcur->lc;
    }

    return pcur;
}

static void i_adjust(tl_rbtree_root_t *root, rb_node_t *pcur){

    rb_node_t *grandf = NULL;
    rb_node_t *father = NULL;

    while(true){

        father = pcur->father;

        if(NULL == father || RB_BLACK == father->color){
            break;
        }

        grandf = father->father;
        tl_assert(NULL != grandf);

        if(grandf->lc->color == grandf->rc->color){
            tl_assert(RB_RED == grandf->lc->color);

            grandf->lc->color = RB_BLACK;
            grandf->rc->color = RB_BLACK;
            grandf->color = RB_RED;
            pcur = grandf;

        }else{

            if(father == grandf->lc && pcur == father->rc){

                l_rotate(root, father);

                pcur = pcur->lc;
                tl_assert(father == pcur);
                father = pcur->father;

            }else if(father == grandf->rc && pcur == father->lc){

                r_rotate(root, father);

                pcur = pcur->rc;
                tl_assert(father == pcur);
                father = pcur->father;
            }

            grandf->color = RB_RED;
            father->color = RB_BLACK;

            if(pcur == father->lc && father == grandf->lc){
                pcur = grandf;
                r_rotate(root, pcur);
            }else{
                tl_assert(pcur == father->rc && father == grandf->rc);
                pcur = grandf;
                l_rotate(root, pcur);
            }
        }
    }

    rb_firstNode(root)->color = RB_BLACK;
}

static void d_adjust(tl_rbtree_root_t *root, rb_node_t *pcur, rb_node_t *father){
    rb_node_t *pbro = NULL;
    bool b_left = true;

    while(rb_firstNode(root) != pcur && ((!isforge(pcur) && RB_BLACK == pcur->color) || isforge(pcur))){

        pbro = brother(father, pcur, &b_left);

        tl_assert(!isforge(pbro));

        if(RB_RED == pbro->color){

            tl_assert(RB_BLACK == pbro->father->color);

            rb_node_t *ptmp = pbro->father;

            pbro->father->color = RB_RED;
            pbro->color = RB_BLACK;

            if(b_left){ l_rotate(root, pbro->father); }
            else { r_rotate(root, pbro->father); }

            pbro = brother(ptmp, pcur, &b_left);
        }

        tl_assert(RB_BLACK == pbro->color);

        if(RB_BLACK == pbro->lc->color && RB_BLACK == pbro->rc->color){
            pbro->color = RB_RED;
            pcur = pbro->father;
        }else{
            rb_node_t *ptmp = pbro->father;
            if(b_left){
                if(RB_BLACK == pbro->rc->color){
                    pbro->lc->color = RB_BLACK;
                    pbro->color = RB_RED;
                    r_rotate(root, pbro);

                    pbro = brother(ptmp, pcur, &b_left);
                }

                pbro->color			= pbro->father->color;
                pbro->father->color = RB_BLACK;
                pbro->rc->color		= RB_BLACK;

                l_rotate(root, pbro->father);
            }else{
                if(RB_BLACK == pbro->lc->color){
                    pbro->rc->color = RB_BLACK;
                    pbro->color = RB_RED;
                    l_rotate(root, pbro);
                    pbro = brother(ptmp, pcur, &b_left);
                }
                pbro->color = pbro->father->color;
                pbro->father->color = RB_BLACK;
                pbro->lc->color = RB_BLACK;
                r_rotate(root, pbro->father);
            }
            pcur = rb_firstNode(root);
        }

        tl_assert(!isforge(pcur));
        father = pcur->father;
    }

    pcur->color = RB_BLACK;
}

tl_rbtree_root_t *tl_rbtree_init(const rb_config_t *prbc){
    if(NULL == prbc) { return NULL; }
    if(NULL == prbc->keycmp) { return NULL; }

#if !(TL_USE_POOL)
    tl_rbtree_root_t *root = (tl_rbtree_root_t *) malloc(sizeof(tl_rbtree_root_t));
    if(NULL == root) { return NULL; }
#else
    tl_pool_t *tp = tl_pool_init();
    if(NULL == tp) { return NULL; }

    tl_rbtree_root_t *root = (tl_rbtree_root_t *) tl_pool_alloc(tp, sizeof(tl_rbtree_root_t));
    if(NULL == root) { 
        tl_pool_destroy(tp);
        return NULL; 
    }
    root->tp = tp;
#endif

    root->rbc = *prbc;

    root->firstN = NULL;
    root->freeN = NULL;
    root->cnt_key = 0;
    return root;
}

int tl_rbtree_insert(tl_rbtree_root_t *root, tl_keyval_t *pkv, bool b_update){

    rb_node_t *newNode = NULL;
    locate_t loc = locateKey(root, pkv->key);

    if(L_UNEXIST == loc.type){
        newNode = trb_alloc_node(root, pkv, NULL, RB_BLACK);
        if(NULL == newNode) { return -1; }
        rb_firstNode(root) = newNode;
        return 0;
    }else if(L_LOCATE == loc.type){
        if(b_update){
            pkv->val = loc.loc->val;
            return 0;
        }
        return -1;
    }else{
        rb_node_t *pcur = loc.loc;
        newNode = trb_alloc_node(root, pkv, pcur, RB_RED);
        if(NULL == newNode) { return -1; }

        if(L_LCHILD == loc.type){
            pcur->lc = newNode;
        }else{
            pcur->rc = newNode;
        }

        i_adjust(root, newNode);
        return 0;
    }
}

int tl_rbtree_query(tl_rbtree_root_t *root, tl_keyval_t *pkv){
    locate_t loc = locateKey(root, pkv->key);
    if(L_LOCATE == loc.type){
        pkv->val = loc.loc->val;
        return 0;
    }
    return -1;
}

int tl_rbtree_erase(tl_rbtree_root_t *root, tl_keyval_t *pkv){
    locate_t loc = locateKey(root, pkv->key);
    if(L_LOCATE != loc.type) { return -1; }
    else{

        rb_node_t *pcur		= loc.loc;
        rb_node_t *pdel		= NULL;
        rb_node_t *father	= NULL;
        rb_node_t *pu		= NULL;

        pkv->val = pcur->val;

        if(!isforge(pcur->lc) && !isforge(pcur->rc)){ 
            pdel = succeed(pcur);
            pcur->key = pdel->key; 
            pcur->val = pdel->val;
        }else{
            pdel = pcur;
        }

        if(!isforge(pdel->lc)) { pu = pdel->lc; }
        else { pu = pdel->rc; }

        father = pdel->father;

        if(NULL == father){
            tl_assert(rb_firstNode(root) == pdel);

            rb_firstNode(root) = pu;
            if(isforge(rb_firstNode(root))){
                rb_firstNode(root) = NULL;
                goto L_return;
            }
        }else{
            if(pdel == father->lc){
                father->lc = pu;
            }else{
                tl_assert(pdel == father->rc);
                father->rc = pu;
            }
        }

        if(!isforge(pu)) { pu->father = father; }

        if(RB_BLACK == pdel->color) { d_adjust(root, pu, father); }

L_return:

        trb_free_node(root, pdel);
        return 0;
    }
}

#if !(TL_USE_POOL)

static void destroy(rb_node_t *node){
    if(!isforge(node)){
        destroy(node->lc);
        destroy(node->rc);
        free(node);
    }
}

#endif

void  tl_rbtree_destroy(tl_rbtree_root_t *root){

#if !(TL_USE_POOL)
    if(NULL == root) { return; }
    rb_node_t *node = rb_firstNode(root);
    if(NULL != node) {
        destroy(node);
    }
    free(root);
#else
    tl_pool_destroy(root->tp);
#endif
}

size_t tl_rbtree_getcnt(const tl_rbtree_root_t *root){
    return root->cnt_key;
}

int tl_rbtree_first(tl_rbtree_root_t *root, tl_rbtree_entry_t *entry){
    if(NULL != root->firstN){
        entry->ploc = root->firstN;
        while(!isforge(entry->ploc->lc)){
            entry->ploc = entry->ploc->lc;
        }
        return 0;
    }
    return -1;
}

int tl_rbtree_last(tl_rbtree_root_t *root, tl_rbtree_entry_t *entry){
    if(NULL != root->firstN){
        entry->ploc = root->firstN;
        while(!isforge(entry->ploc->rc)){
            entry->ploc = entry->ploc->rc;
        }
        return 0;
    }
    return -1;
}

int tl_rbtree_next(tl_rbtree_entry_t *entry){	
    rb_node_t *p = entry->ploc;
    if(!isforge(p->rc)){
        p = p->rc;
        while(!isforge(p->lc)){
            p = p->lc;
        }
        entry->ploc = p;
        return 0;
    }else{
        while(NULL != p->father){
            if(p != p->father->rc){
                entry->ploc = p->father;
                return 0;
            }
            p = p->father;
        }
        return -1;
    }
}

int tl_rbtree_prev(tl_rbtree_entry_t *entry){
    rb_node_t *p = entry->ploc;
    if(!isforge(p->lc)){
        p = p->lc;
        while(!isforge(p->rc)){
            p = p->rc;
        }
        entry->ploc = p;
        return 0;
    }else{
        while(NULL != p->father){
            if(p != p->father->lc){
                entry->ploc = p->father;
                return 0;
            }
            p = p->father;
        }
        return -1;
    }
}

void tl_rbtree_extract(tl_rbtree_entry_t *entry, tl_keyval_t *pkv){
    pkv->key = entry->ploc->key;
    pkv->val = entry->ploc->val;
}

void tl_rbtree_set(tl_rbtree_entry_t *entry, tl_val_t val){
    entry->ploc->val = val;
}

static int check(const tl_rbtree_root_t *root, const rb_node_t *node){
    int hgt = RB_BLACK == node->color ? 1 : 0;
    int hgtl = 0;
    int hgtr = 0;
    if(!isforge(node)){
        if(!isforge(node->lc)){
            if(-1 != (rb_keycmp(root)(node->lc->key, node->key))){ return -1; }
        }

        if(!isforge(node->rc)){
            if(1 != (rb_keycmp(root)(node->rc->key, node->key))){ return -1; }
        }
        hgtl = check(root, node->lc);
        hgtr = check(root, node->rc);
        if((-1 == hgtl || -1 == hgtr) || (hgtl != hgtr)){
            return -1;
        }
    }
    return hgt + hgtl;
}

int tl_rbtree_check(const tl_rbtree_root_t *root){
    if(NULL == root) return 0;
    const rb_node_t *node = rb_firstNode(root);
    int hgt;
    if(NULL == node) return 0;
    if(RB_BLACK != node->color) { return -1; }
    hgt = check(root, node);
    return 0;
}
