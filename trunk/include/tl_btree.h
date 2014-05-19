
/*
 *      Copyright (C) yangnian
 *      Email :  ydyangnian@163.com
 * */

#ifndef TOOL_BPLUSTREE_H
#define TOOL_BPLUSTREE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include "tl_common.h"

    typedef struct {
        int m_fork;
        keycmpHandle keycmp;
    }tl_bplustree_config_t;
	
	struct tl_bplustree_entry_t;
	typedef struct tl_bplustree_entry_t tl_bplustree_entry_t;

    struct tl_bplustree_root_t;
    typedef struct tl_bplustree_root_t tl_bplustree_root_t;

    tl_bplustree_root_t *tl_bplustree_init(const tl_bplustree_config_t *);
    int tl_bplustree_insert	(tl_bplustree_root_t *p, tl_keyval_t *pkv, bool b_update);
    int tl_bplustree_query	(tl_bplustree_root_t *p, tl_keyval_t *pkv);
    int tl_bplustree_erase	(tl_bplustree_root_t *p, tl_keyval_t *pkv);
    int tl_bplustree_destroy(tl_bplustree_root_t *p);
    
	int tl_bplustree_first	(tl_bplustree_root_t *, tl_bplustree_entry_t *);
	int tl_bplustree_last	(tl_bplustree_root_t *, tl_bplustree_entry_t *);

	int tl_bplustree_next	(tl_bplustree_entry_t *);
	int tl_bplustree_prev	(tl_bplustree_entry_t *);

    tl_keyval_t  tl_bplustree_get(const tl_bplustree_root_t *p, int idx);
    uintptr_t tl_bplustree_getcnt(const tl_bplustree_root_t *);

#ifdef __cplusplus
}
#endif

#endif
