#include "gpio_irq_hardware_mock.h"

#include <stddef.h>

#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "pico/platform.h"

static gpio_irq_callback_t callback;
static uint32_t enabled_events[NUM_BANK0_GPIOS];
static bool bank_enabled;
static bool interrupts_enabled;
static unsigned int current_core;
static unsigned int callback_set_count;

void gpio_irq_hardware_mock_reset(void)
{
    callback = NULL;
    for (size_t gpio = 0U; gpio < NUM_BANK0_GPIOS; ++gpio) {
        enabled_events[gpio] = 0U;
    }
    bank_enabled = false;
    interrupts_enabled = true;
    current_core = 0U;
    callback_set_count = 0U;
}

bool gpio_irq_hardware_mock_interrupts_enabled(void)
{
    return interrupts_enabled;
}

unsigned int gpio_irq_hardware_mock_callback_set_count(void)
{
    return callback_set_count;
}

void gpio_irq_hardware_mock_deliver(unsigned int gpio, uint32_t events)
{
    if (callback != NULL) {
        callback(gpio, events);
    }
}

void gpio_irq_hardware_mock_set_core(unsigned int core)
{
    current_core = core;
}

bool gpio_irq_hardware_mock_bank_enabled(void)
{
    return bank_enabled;
}

bool gpio_irq_hardware_mock_pin_enabled(
    unsigned int gpio,
    uint32_t events)
{
    return gpio < NUM_BANK0_GPIOS
        && (enabled_events[gpio] & events) == events;
}

bool gpio_irq_hardware_mock_fire(
    unsigned int gpio,
    uint32_t events)
{
    if (!bank_enabled
            || !interrupts_enabled
            || callback == NULL
            || gpio >= NUM_BANK0_GPIOS) {
        return false;
    }

    const uint32_t relevant_events = enabled_events[gpio] & events;
    if (relevant_events == 0U) {
        return false;
    }

    callback(gpio, relevant_events);
    return true;
}

void gpio_set_irq_callback(gpio_irq_callback_t new_callback)
{
    callback_set_count++;
    callback = new_callback;
}

void gpio_set_irq_enabled(
    unsigned int gpio,
    uint32_t event_mask,
    bool enabled)
{
    if (gpio >= NUM_BANK0_GPIOS) {
        return;
    }

    if (enabled) {
        enabled_events[gpio] |= event_mask;
    } else {
        enabled_events[gpio] &= ~event_mask;
    }
}

void irq_set_enabled(unsigned int interrupt, bool enabled)
{
    if (interrupt == IO_IRQ_BANK0) {
        bank_enabled = enabled;
    }
}

uint32_t save_and_disable_interrupts(void)
{
    const uint32_t previous = interrupts_enabled ? 1U : 0U;
    interrupts_enabled = false;
    return previous;
}

void restore_interrupts(uint32_t status)
{
    interrupts_enabled = status != 0U;
}

unsigned int get_core_num(void)
{
    return current_core;
}
