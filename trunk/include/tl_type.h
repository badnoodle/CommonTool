#ifndef TOOL_TYPE_H
#define TOOL_TYPE_H

#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

/*
 * -1       ptr1 < ptr2
 *  0       ptr1 == ptr2
 *  1       ptr1 > ptr2
 * */

typedef int (*keycmpHandle) (const void *, const void *);

typedef void *	tl_key_t;
typedef void *	tl_val_t;

typedef struct {
	tl_key_t key;
	tl_val_t val;
}tl_keyval_t;

#endif

