#include "test_harness.h"

#include <stdlib.h>

int t_failures;
int t_checks;
int t_cases;
char t_case_context[256];
const char *t_current_case = "";
bool t_case_failed;
bool t_dump_trace_on_failure = true;

void t_context(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    (void)vsnprintf(t_case_context, sizeof(t_case_context), format, args);
    va_end(args);
}

void t_clear_context(void)
{
    t_case_context[0] = '\0';
}

const char *t_result_name(block_device_result_t result)
{
    switch (result) {
    case BLOCK_DEVICE_RESULT_OK: return "OK";
    case BLOCK_DEVICE_RESULT_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case BLOCK_DEVICE_RESULT_NOT_INITIALIZED: return "NOT_INITIALIZED";
    case BLOCK_DEVICE_RESULT_OUT_OF_RANGE: return "OUT_OF_RANGE";
    case BLOCK_DEVICE_RESULT_IO_ERROR: return "IO_ERROR";
    case BLOCK_DEVICE_RESULT_BUSY_TIMEOUT: return "BUSY_TIMEOUT";
    case BLOCK_DEVICE_RESULT_INVALID_DEVICE: return "INVALID_DEVICE";
    case BLOCK_DEVICE_RESULT_NOT_IMPLEMENTED: return "NOT_IMPLEMENTED";
    default: return "<unknown>";
    }
}

void t_report_failure(const char *file, int line, const char *text)
{
    t_failures++;
    t_case_failed = true;
    (void)fprintf(stderr, "\n%s:%d: FAILED in \"%s\"\n", file, line,
        t_current_case);
    if (t_case_context[0] != '\0') {
        (void)fprintf(stderr, "  case: %s\n", t_case_context);
    }
    (void)fprintf(stderr, "  check: %s\n", text);
    if (t_dump_trace_on_failure && sd_card_trace_length() > 0U) {
        sd_card_trace_dump(stderr, 60U);
    }
}

void t_run(void (*test)(void), const char *name)
{
    const int before = t_failures;
    t_cases++;
    t_current_case = name;
    t_case_failed = false;
    t_clear_context();
    test();
    (void)printf("%s %s\n", t_failures == before ? "PASS" : "FAIL", name);
    t_clear_context();
}

int t_summary(const char *suite)
{
    if (t_failures != 0) {
        (void)fprintf(stderr, "\n%s: %d check(s) failed across %d case(s)\n",
            suite, t_failures, t_cases);
        return 1;
    }
    (void)printf("%s: all %d cases passed (%d checks)\n",
        suite, t_cases, t_checks);
    return 0;
}
