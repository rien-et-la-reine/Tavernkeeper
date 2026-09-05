#ifndef TAVERNKEEP_TEST_CHECK_H
#define TAVERNKEEP_TEST_CHECK_H
#include <stdio.h>
#include <stdlib.h>
/* Do not use assert(): checks must also run in Release/NDEBUG builds. */
#define REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)
#endif
