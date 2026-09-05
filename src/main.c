#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"

#include "platform/board.h"
#include "platform/debug.h"
#include "platform/gpio_irq.h"

enum {
    STATUS_LED_TOGGLE_INTERVAL_MS = 500,
    MAIN_LOOP_IDLE_INTERVAL_MS = 10,
};

int main(void)
{
    board_init();

    const bool debug_ready = debug_init();
    if (debug_ready) {
        DEBUG_INFO("Tavernkeep RP2350 firmware");
        DEBUG_INFO("Pico 2 Cortex-M33 bring-up complete; MCU is running");
    }

    //initialize gpio interrupt handling
    if (!platform_gpio_irq_init()) {
        //already initialized on other core
        DEBUG_INFO("a second core tried to initialize the gpio interrupt dispatcher, which already exists on another core");
    }

    bool status_led_on = false;
    absolute_time_t next_led_toggle =
        make_timeout_time_ms(STATUS_LED_TOGGLE_INTERVAL_MS);

    /*
     * Cooperative foreground loop. Future subsystem state machines can be
     * serviced here; interrupts, DMA, and PIO should handle time-critical I/O.
     * TODO(storage): Add a foreground storage coordinator that observes
     * latched media removal, cancels storage consumers, waits for active
     * operations to unwind, and requests idempotent backend teardown.
     */
    while (true) {
        if (board_status_led_available() && time_reached(next_led_toggle)) {
            status_led_on = !status_led_on;
            (void)board_status_led_set(status_led_on);
            next_led_toggle =
                make_timeout_time_ms(STATUS_LED_TOGGLE_INTERVAL_MS);
        }

        sleep_ms(MAIN_LOOP_IDLE_INTERVAL_MS);
    }
}
