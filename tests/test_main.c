#include <setjmp.h>
#include <stdarg.h>
#include <string.h>
#include "pico/stdlib.h"
#include "platform/board.h"
#include "platform/debug.h"
#include "platform/gpio_irq.h"
#include "test_check.h"

int firmware_main(void);
static jmp_buf end_loop;
static uint64_t now;
static unsigned int stage, sleeps, toggles, banners, irq_warnings;
static bool led_available = true, debug_ready = true, irq_ready = true;

void board_init(void) { REQUIRE(stage++ == 0); }
bool debug_init(void) { REQUIRE(stage++ == 1); return debug_ready; }
bool platform_gpio_irq_init(void) { REQUIRE(stage++ == 2); return irq_ready; }
bool board_status_led_available(void) { return led_available; }
bool board_status_led_set(bool on)
{
    REQUIRE(led_available && stage == 3);
    toggles++;
    REQUIRE(now == (uint64_t)toggles * 500);
    REQUIRE(on == ((toggles % 2) != 0));
    return true;
}
void debug_log(debug_level_t level, const char *format, ...)
{
    REQUIRE(level == DEBUG_LEVEL_INFO && format != NULL);
    if (stage == 2) {
        REQUIRE(debug_ready);
        banners++;
    } else {
        REQUIRE(stage == 3 && !irq_ready);
        irq_warnings++;
    }
}
absolute_time_t make_timeout_time_ms(uint32_t ms)
{
    REQUIRE(stage == 3 && ms == 500);
    return now + ms;
}
bool time_reached(absolute_time_t target) { return now >= target; }
void sleep_ms(uint32_t ms)
{
    REQUIRE(stage == 3 && ms == 10);
    now += ms;
    if (++sleeps == 110) { longjmp(end_loop, 1); }
}
int main(int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "no-led") == 0) { led_available = false; }
        else if (strcmp(argv[1], "debug-failure") == 0) { debug_ready = false; }
        else if (strcmp(argv[1], "irq-failure") == 0) { irq_ready = false; }
        else { return 2; }
    } else if (argc != 1) { return 2; }
    if (setjmp(end_loop) == 0) {
        (void)firmware_main();
        REQUIRE(!"firmware main unexpectedly returned");
    }
    REQUIRE(sleeps == 110 && now == 1100);
    REQUIRE(toggles == (led_available ? 2U : 0U));
    REQUIRE(banners == (debug_ready ? 2U : 0U));
    REQUIRE(irq_warnings == (irq_ready ? 0U : 1U));
    puts("PASS firmware startup and cooperative heartbeat");
    return 0;
}
