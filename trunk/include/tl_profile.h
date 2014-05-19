#ifndef TOOL_PROFILE_H
#define TOOL_PROFILE_H

#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#include "tl_rbtree.h"

#ifdef __cplusplus
extern "C"{
#endif

	typedef struct {
		clock_t cpu;
		struct timeval real;
	}tl_profile_time_t;

	typedef struct {
		const char *func;
		size_t cnt_call;
		tl_profile_time_t total;
	}tl_profile_item_t;

	typedef struct tl_profile_entry_t{
		tl_profile_item_t *item;
		tl_profile_time_t ut;
		size_t level;
		struct tl_profile_entry_t *next;
	}tl_profile_entry_t;

	struct tl_profile_t;
	typedef struct tl_profile_t tl_profile_t;

	struct tl_prostatic_t;
	typedef struct tl_prostatic_t tl_prostatic_t;

	tl_profile_t *tl_profile_init(size_t);
	void tl_profile_destroy(tl_profile_t *);
	void tl_profile_start(tl_profile_t *, const char *);
	void tl_profile_end(tl_profile_t *, const char *);
	const tl_profile_entry_t *tl_profile_getEntry(const tl_profile_t *);
	const tl_profile_item_t *tl_profile_getItem(const tl_profile_t *, const char *);
	
	int tl_profile_first(const tl_profile_t *p, tl_rbtree_entry_t *entry);
	int tl_profile_next(tl_rbtree_entry_t *entry);

#ifdef __cplusplus
}
#endif

#endif
