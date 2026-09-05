#include <stdarg.h>
#include <string.h>
#include "pico/stdlib.h"
#include "platform/debug.h"
#include "test_check.h"

/* printf/vprintf/putchar are renamed only in this test target. */
static char output[512];
static size_t output_size;
static unsigned int init_calls;
static bool init_result;
bool stdio_init_all(void) { ++init_calls; return init_result; }
int test_vprintf(const char *format, va_list args);
int test_printf(const char *format, ...);
int test_putchar(int character);
int test_vprintf(const char *format, va_list args)
{
    int written = vsnprintf(output + output_size, sizeof(output) - output_size, format, args);
    REQUIRE(written >= 0 && (size_t)written < sizeof(output) - output_size);
    output_size += (size_t)written;
    return written;
}
int test_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = test_vprintf(format, args);
    va_end(args);
    return result;
}
int test_putchar(int character)
{
    REQUIRE(output_size + 1 < sizeof(output));
    output[output_size++] = (char)character;
    output[output_size] = '\0';
    return character;
}
int main(void)
{
#if TAVERNKEEP_LOGGING_ENABLED
    REQUIRE(!debug_init());
    init_result = true;
    REQUIRE(debug_init() && init_calls == 2);
    debug_log(DEBUG_LEVEL_INFO, NULL);
    REQUIRE(output_size == 0);
    DEBUG_INFO("value=%d %s", 42, "ok");
    DEBUG_WARN("%s", "warning");
    DEBUG_ERROR("hex=%x", 0xab);
    debug_log((debug_level_t)99, "unknown");
    REQUIRE(strcmp(output, "[INFO] value=42 ok\n[WARN] warning\n[ERROR] hex=ab\n[UNKNOWN] unknown\n") == 0);
#else
    REQUIRE(debug_init() && init_calls == 0);
    int evaluated = 0;
    DEBUG_INFO("%d", ++evaluated);
    DEBUG_WARN("%d", ++evaluated);
    DEBUG_ERROR("%d", ++evaluated);
    debug_log(DEBUG_LEVEL_INFO, "direct call %d", 42);
    debug_log(DEBUG_LEVEL_ERROR, NULL);
    REQUIRE(evaluated == 0 && output_size == 0 && init_calls == 0);
#endif
    puts("PASS logging configuration");
    return 0;
}
