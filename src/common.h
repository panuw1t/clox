#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* #define DEBUG_PRINT_CODE */
/* #define DEBUG_TRACE_EXECUTION */
#define DEBUG_TRACE_FUNCTION

#ifdef DEBUG_TRACE_FUNCTION
static int indent = 0;

#define TRACE_ENTER()                           \
do {                                            \
    printf("%*s%s\n", indent, "", __func__);    \
    indent += 2;                                \
} while (0)

#define TRACE_EXIT()                            \
do {                                            \
    indent -= 2;                                \
    printf("%*s%s\n", indent, "", __func__);    \
} while (0)

#else
#define TRACE_ENTER()
#define TRACE_EXIT()

#endif // DEBUG_TRACE_FUNCTION

#endif /* COMMON_H */
