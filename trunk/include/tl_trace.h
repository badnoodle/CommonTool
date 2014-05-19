#ifndef TOOL_TRACE_H
#define TOOL_TRACE_H

#include "tl_config.h"

#define TL_TRACE_ON 1

#if TL_TRACE_ON

#include <stdio.h>

#define TTL_TRACE	1
#define TTL_WARN	1
#define TTL_ALERT	1
#define TTL_ERROR	1
#define TTL_DEBUG	1
#define TTL_EMERG	1
#define TTL_INFO	1

#define TL_AUTOGEN(level, fmt, ...)	do {\
	if ((TTL_##level)){\
		printf("[%6s] ", __STRING(level));\
		printf(fmt, ##__VA_ARGS__); fflush(stdout);\
	}\
} while(0)

#define tl_trace(fmt , ...)	TL_AUTOGEN(TRACE, fmt, ##__VA_ARGS__)
#define tl_warn(fmt , ...)		TL_AUTOGEN(WARN, fmt, ##__VA_ARGS__)
#define tl_alert(fmt , ...)	TL_AUTOGEN(ALERT, fmt, ##__VA_ARGS__)
#define tl_error(fmt , ...)	TL_AUTOGEN(ERROR, fmt, ##__VA_ARGS__)
#define tl_debug(fmt , ...)	TL_AUTOGEN(DEBUG, fmt, ##__VA_ARGS__)
#define tl_emerg(fmt , ...)	TL_AUTOGEN(EMERG, fmt, ##__VA_ARGS__)
#define tl_info(fmt, ...)		do {\
	if ((TTL_INFO)){\
		printf(fmt, ##__VA_ARGS__); \
	}\
} while(0)

#else

#define tl_trace(fmt , ...)
#define tl_warn(fmt , ...)
#define tl_alert(fmt , ...)
#define tl_error(fmt , ...)
#define tl_debug(fmt , ...)
#define tl_emerg(fmt , ...)
#define tl_info(fmt, ...)

#endif

#define tl_showmem(ptr, len, prefix) \
    do{\
        tl_info("%s", prefix);\
        size_t idx = 0;\
        while(idx < (size_t)(len)){\
            tl_info("%2x ", ((const uint8_t *)(ptr))[idx ++] & 0xff);\
        }\
        tl_info("\n");\
    }while(0)
#define tl_log_here()	tl_info("now in here <%s:%d>\n", __FILE__, __LINE__)
#endif
