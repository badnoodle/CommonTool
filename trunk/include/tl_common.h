
#ifndef TOOL_COMMON_H
#define TOOL_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tl_config.h"

/*
 *	other standard c89 headers
 * */

#include <assert.h>
#include <locale.h>
#include <stddef.h> 
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <errno.h> 
#include <setjmp.h>
#include <stdlib.h>
#include <float.h>
#include <signal.h> 
#include <string.h>
#include <limits.h>
#include <stdarg.h>
#include <time.h> 

/*
 *	other standard c99 headers
 * */

/*
 *	other used headers
 * */

#ifdef LINUX
    #include <alloca.h>
    #include <malloc.h>
#endif

#include <sched.h>
#include <unistd.h>
#include <pthread.h>

#include "tl_type.h"
#include "tl_trace.h"
#include "tl_assert.h"

#ifndef NULL
#define NULL (void *)0
#endif

#define tl_max(a, b)	(((a) < (b)) ? (b) : (a))
#define tl_min(a, b)	(((a) < (b)) ? (a) : (b))

#define tl_offsetof	offsetof
#define tl_2string	__STRING
#define tl_yield	sched_yield

extern uintptr_t tl_pagesize;
extern struct tl_pool_t *afx_pool;

#ifdef FREEBSD
void *tl_memalign(size_t, size_t);
#endif

#ifdef LINUX
#define tl_memalign memalign
#endif

#ifdef __cplusplus
}
#endif

#endif
