#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"

#include "platform/board.h"
#include "platform/debug.h"

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

    bool status_led_on = false;
    absolute_time_t next_led_toggle =
        make_timeout_time_ms(STATUS_LED_TOGGLE_INTERVAL_MS);

    /*
     * Cooperative foreground loop. Future subsystem state machines can be
     * serviced here; interrupts, DMA, and PIO should handle time-critical I/O.
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

