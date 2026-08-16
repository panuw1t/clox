#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UINT8_COUNT (UINT8_MAX + 1)

#define DEBUG_PRINT_CODE
/* #define DEBUG_TRACE_EXECUTION */

#ifdef DEBUG_TRACE_EXECUTION
    static int trace_depth = 0;

    #define TRACE_ENTRY(fmt, ...) do { \
        for(int i=0; i<trace_depth; i++) printf("  "); \
        printf("%s" fmt "\n", __func__, ##__VA_ARGS__); \
        trace_depth++; \
    } while(0)

    #define TRACE_EXIT() do { \
        trace_depth--; \
        for(int i=0; i<trace_depth; i++) printf("  "); \
        printf("%s\n", __func__); \
    } while(0)
#else
    #define TRACE_ENTRY(fmt, ...)
    #define TRACE_EXIT()
#endif

#endif /* COMMON_H */
