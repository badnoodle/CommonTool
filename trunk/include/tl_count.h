#ifndef TOOL_COUNT_H
#define TOOL_COUNT_H

#include "tl_common.h"

enum {
	TL_C_EQ = 1,
	TL_C_LT = 2,
	TL_C_GT = 4,
	TL_C_LE = TL_C_EQ | TL_C_LT,
	TL_C_GE = TL_C_EQ | TL_C_GT,
	TL_C_INVALID
};

#define tl_count_digit2str(digit)	__STRING(digit)
#define tl_count_var2str(var)	

struct tl_count_t;
typedef struct tl_count_t tl_count_t;

#ifdef __cplusplus
extern "C" {
#endif

int tl_count_set(tl_count_t *, const char *);

bool tl_count_cmp(int type, const tl_count_t *, const tl_count_t *);

int tl_count_add(tl_count_t *, const char *);

int tl_count_sub(tl_count_t *, const char *);

int tl_count_inc(tl_count_t *);

int tl_count_dec(tl_count_t *);

#ifdef __cplusplus
}
#endif

#endif