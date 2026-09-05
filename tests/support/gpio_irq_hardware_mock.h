#ifndef TAVERNKEEP_TEST_GPIO_IRQ_HARDWARE_MOCK_H
#define TAVERNKEEP_TEST_GPIO_IRQ_HARDWARE_MOCK_H

#include <stdbool.h>
#include <stdint.h>

void gpio_irq_hardware_mock_reset(void);
void gpio_irq_hardware_mock_set_core(unsigned int core);
bool gpio_irq_hardware_mock_interrupts_enabled(void);
unsigned int gpio_irq_hardware_mock_callback_set_count(void);
/* Simulate SDK delivery directly, including stale/unsubscribed event bits. */
void gpio_irq_hardware_mock_deliver(unsigned int gpio, uint32_t events);
bool gpio_irq_hardware_mock_bank_enabled(void);
bool gpio_irq_hardware_mock_pin_enabled(
    unsigned int gpio,
    uint32_t events);
bool gpio_irq_hardware_mock_fire(
    unsigned int gpio,
    uint32_t events);

#endif
