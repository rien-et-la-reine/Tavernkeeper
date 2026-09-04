#ifndef TAVERNKEEP_TEST_FAKE_HARDWARE_GPIO_H
#define TAVERNKEEP_TEST_FAKE_HARDWARE_GPIO_H

#include <stdbool.h>

enum {
    GPIO_IN = 0,
    GPIO_OUT = 1,
    GPIO_FUNC_SPI = 1,
};

void gpio_init(unsigned int pin);
void gpio_deinit(unsigned int pin);
void gpio_pull_up(unsigned int pin);
void gpio_put(unsigned int pin, bool value);
bool gpio_get(unsigned int pin);
void gpio_set_dir(unsigned int pin, bool out);
void gpio_set_function(unsigned int pin, unsigned int function);

#endif
