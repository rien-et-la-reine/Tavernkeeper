#ifndef TAVERNKEEP_TEST_FAKE_HARDWARE_GPIO_H
#define TAVERNKEEP_TEST_FAKE_HARDWARE_GPIO_H

#include <stdbool.h>
#include <stdint.h>

enum {
    NUM_BANK0_GPIOS = 48,
    GPIO_IN = 0,
    GPIO_OUT = 1,
    GPIO_FUNC_SPI = 1,
    GPIO_IRQ_LEVEL_LOW = 0x1U,
    GPIO_IRQ_LEVEL_HIGH = 0x2U,
    GPIO_IRQ_EDGE_FALL = 0x4U,
    GPIO_IRQ_EDGE_RISE = 0x8U,
};

typedef void (*gpio_irq_callback_t)(
    unsigned int gpio,
    uint32_t event_mask);

void gpio_init(unsigned int pin);
void gpio_deinit(unsigned int pin);
void gpio_pull_up(unsigned int pin);
void gpio_put(unsigned int pin, bool value);
bool gpio_get(unsigned int pin);
void gpio_set_dir(unsigned int pin, bool out);
void gpio_set_function(unsigned int pin, unsigned int function);
void gpio_set_irq_callback(gpio_irq_callback_t callback);
void gpio_set_irq_enabled(
    unsigned int pin,
    uint32_t event_mask,
    bool enabled);

#endif
