
#ifndef TOOL_BTREE_H
#define TOOL_BTREE_H


#ifdef __cplusplus
extern "C" {
#endif

	#include "tl_common.h"
	
	struct tl_btree_entry_t;
	typedef struct tl_btree_entry_t tl_btree_entry_t;

	struct tl_btree_root_t;
	typedef struct tl_btree_root_t tl_btree_root_t;

	typedef struct {
		int m_fork;
		keycmpHandle keycmp;
	}tl_btree_config_t;
	
	size_t tl_btree_entrySize();

	tl_btree_root_t * tl_btree_init(const tl_btree_config_t *);
	int tl_btree_insert(tl_btree_root_t *, tl_keyval_t *, bool );
	int tl_btree_erase(tl_btree_root_t *, tl_keyval_t *);
	int tl_btree_query(tl_btree_root_t *, tl_keyval_t *);
	void tl_btree_destroy(tl_btree_root_t *);
	size_t tl_btree_getcnt(tl_btree_root_t *);
	void tl_btree_show(tl_btree_root_t *);
	int tl_btree_check(tl_btree_root_t *);

	int tl_btree_first(tl_btree_root_t *, tl_btree_entry_t *);
	int tl_btree_last(tl_btree_root_t *, tl_btree_entry_t *);
	int tl_btree_next(tl_btree_entry_t *);
	int tl_btree_prev(tl_btree_entry_t *);

	void tl_btree_extract(tl_btree_entry_t *, tl_keyval_t *);
	void tl_btree_set(tl_btree_entry_t *, tl_val_t);

#ifdef __cplusplus
}
#endif

#endif
