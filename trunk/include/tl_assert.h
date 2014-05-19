#ifndef TOOL_ASSERT_H
#define TOOL_ASSERT_H

#include "tl_config.h"

#if TL_ASSERT_ON

#include <assert.h>

#ifdef assert
#undef assert
#endif

/*
 *	tl_assert usage:
 *		number of marco tl_assert only be 1 or 2
 *		the second parameter must be a function or block
 *		others may cause compile error
 * */

#ifdef FREEBSD

#define tl_assert(expr, ...) \
    ((expr) ? (void)0\
     : \
     (({ __VA_ARGS__; }),  __assert(__func__, __FILE__, __LINE__, #expr)))

#endif

#ifdef LINUX

#define tl_assert(expr, ...) \
    ((expr) ? __ASSERT_VOID_CAST(0)\
	: \
	( ({ __VA_ARGS__; }), __assert_fail(__STRING((expr)), __FILE__, __LINE__, __ASSERT_FUNCTION)))

#endif

#else

#define tl_assert(expr, ...) (expr)

#endif

#endif

