#ifndef _TRACE_H_
#define _TRACE_H_

#include <stdio.h>

#define DEBUG_ETH  1

extern void trace_init(void);

#if DEBUG_ETH == 1

#define TRACE(fmt, ...)  printf(fmt, ##__VA_ARGS__)

#define SIZE_PER_LINE             16
#define TRACE_ARRAY(arr, len)                         \
do {                                                  \
    UINT32 length = len;                              \
    UINT8 *p = (UINT8 *)arr;                          \
    while (length >= SIZE_PER_LINE) {                 \
        for (UINT32 i = 0; i < SIZE_PER_LINE; i++) {  \
            TRACE("0x%02X ", *p++);                   \
        }                                             \
        length -= SIZE_PER_LINE;                      \
        TRACE("\n");                                  \
    }                                                 \
    if (length) {                                     \
        for (UINT32 i = 0; i < length; i++) {         \
            TRACE("0x%02X ", *p++);                   \
        }                                             \
        TRACE("\n");                                  \
    }                                                 \
} while (0)

#define TRACE_ARRAY_DELIMIT(arr, len, delimit)            \
do {                                                      \
    UINT32 length = len;                                  \
    UINT8 *p = (UINT8 *)arr;                              \
    while (length >= SIZE_PER_LINE) {                     \
        for (UINT32 i = 0; i < SIZE_PER_LINE - 1; i++) {  \
            TRACE("0x%02X%c", *p++, delimit);             \
        }                                                 \
        TRACE("0x%02X", *p++);                            \
        length -= SIZE_PER_LINE;                          \
        TRACE("\n");                                      \
    }                                                     \
    if (length) {                                         \
        for (UINT32 i = 0; i < length - 1; i++) {         \
            TRACE("0x%02X%c", *p++, delimit);             \
        }                                                 \
        TRACE("0x%02X", *p++);                            \
        TRACE("\n");                                      \
    }                                                     \
} while (0)

#else
#define TRACE(fmt, ...)
#define TRACE_ARRAY(arr, len)
#define TRACE_ARRAY_DELIMIT(arr, len, delimit)
#endif

#endif
