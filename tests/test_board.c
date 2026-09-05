#include "pico/stdlib.h"
#include "platform/board.h"
#include "test_check.h"

static unsigned int init_calls, direction_calls, put_calls;
static unsigned int last_pin;
static bool last_level, last_direction;
void gpio_init(unsigned int pin) { ++init_calls; last_pin = pin; }
void gpio_set_dir(unsigned int pin, bool out) { ++direction_calls; last_pin = pin; last_direction = out; }
void gpio_put(unsigned int pin, bool value) { ++put_calls; last_pin = pin; last_level = value; }

int main(void)
{
    board_init();
#if defined(PICO_DEFAULT_LED_PIN)
    REQUIRE(board_status_led_available());
    REQUIRE(init_calls == 1 && direction_calls == 1 && put_calls == 1);
    REQUIRE(last_pin == PICO_DEFAULT_LED_PIN && last_direction == GPIO_OUT);
#if defined(PICO_DEFAULT_LED_PIN_INVERTED) && PICO_DEFAULT_LED_PIN_INVERTED
    const bool off_level = true;
#else
    const bool off_level = false;
#endif
    REQUIRE(last_level == off_level);
    REQUIRE(board_status_led_set(true));
    REQUIRE(last_level != off_level && last_pin == PICO_DEFAULT_LED_PIN);
    REQUIRE(board_status_led_set(false));
    REQUIRE(last_level == off_level && put_calls == 3);
    REQUIRE(init_calls == 1 && direction_calls == 1);
#else
    REQUIRE(!board_status_led_available());
    REQUIRE(!board_status_led_set(true));
    REQUIRE(!board_status_led_set(false));
    REQUIRE(init_calls == 0 && direction_calls == 0 && put_calls == 0);
#endif
    puts("PASS board LED configuration");
    return 0;
}
