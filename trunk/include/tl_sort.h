
/*
 *      Copyright (C) yangnian
 *      Email:  ydyangnian@163.com
 * */

#ifndef TOOL_SORT_H
#define TOOL_SORT_H

#ifdef __cplusplus
extern "C" {
#endif

	#include "tl_common.h"

	typedef struct {
		void *array;        //  element container
		size_t eleSize;     //  each element size
		size_t eleCnt;      //  total element count
		keycmpHandle cmp;   //  
	}sort_config_t;

	/*
	 *      quick sort
	 *      no optimize
	 * */
	void tl_q_sort(sort_config_t *);

	/*
	 *      heap sort
	 * */
	void tl_h_sort(sort_config_t *);

	void tl_i_sort(sort_config_t *);

#ifdef __cplusplus
}
#endif

#endif
