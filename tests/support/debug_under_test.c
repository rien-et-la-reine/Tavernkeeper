/* Include headers before substitution so GCC's format(printf) stays intact. */
#include <stdio.h>
#include <stdarg.h>
#include "platform/debug.h"
int test_printf(const char *format, ...);
int test_vprintf(const char *format, va_list args);
int test_putchar(int character);
#define printf test_printf
#define vprintf test_vprintf
#define putchar test_putchar
#include "../../src/platform/debug.c"
