#include "platform/board.h"

#include "pico/stdlib.h"

void board_init(void)
{
#if defined(PICO_DEFAULT_LED_PIN)
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    (void)board_status_led_set(false);
#endif
}

bool board_status_led_available(void)
{
#if defined(PICO_DEFAULT_LED_PIN)
    return true;
#else
    return false;
#endif
}

bool board_status_led_set(bool on)
{
#if defined(PICO_DEFAULT_LED_PIN)
#if defined(PICO_DEFAULT_LED_PIN_INVERTED) && PICO_DEFAULT_LED_PIN_INVERTED
    gpio_put(PICO_DEFAULT_LED_PIN, !on);
#else
    gpio_put(PICO_DEFAULT_LED_PIN, on);
#endif
    return true;
#else
    (void)on;
    return false;
#endif
}

