
#ifndef LIB_RBTREE_H
#define LIB_RBTREE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tl_common.h"

    typedef struct {
        keycmpHandle keycmp;
    }rb_config_t;

    struct tl_rbtree_entry_t;
    typedef struct tl_rbtree_entry_t tl_rbtree_entry_t;

    struct tl_rbtree_root_t;
    typedef struct tl_rbtree_root_t tl_rbtree_root_t;

    size_t tl_rbtree_entrySize();

    tl_rbtree_root_t *tl_rbtree_init(const rb_config_t *prbc);
    int tl_rbtree_insert(tl_rbtree_root_t *root, tl_keyval_t *pkv, bool b_update);
    void tl_rbtree_destroy(tl_rbtree_root_t *root);
    int tl_rbtree_query(tl_rbtree_root_t *root, tl_keyval_t *pkv);
    int tl_rbtree_erase(tl_rbtree_root_t *root, tl_keyval_t *pkv);

    int tl_rbtree_first(tl_rbtree_root_t *, tl_rbtree_entry_t *);
    int tl_rbtree_last(tl_rbtree_root_t *, tl_rbtree_entry_t *);
    int tl_rbtree_next(tl_rbtree_entry_t *);
    int tl_rbtree_prev(tl_rbtree_entry_t *);

    void tl_rbtree_extract(tl_rbtree_entry_t *, tl_keyval_t *);
    void tl_rbtree_set(tl_rbtree_entry_t *, tl_val_t );

    /*
     *  FIXME
     *          impl in later
     * */
    size_t tl_rbtree_getcnt(const tl_rbtree_root_t *root);
    int tl_rbtree_check(const tl_rbtree_root_t *root);
#ifdef __cplusplus
}
#endif

#endif
