#include "platform/gpio_irq.h"

#include <stddef.h>

#include "hardware/gpio.h"
#include "pico_mock.h"

typedef struct {
    platform_gpio_irq_handler_t handler;
    void *context;
    uint32_t events;
    bool enabled;
    size_t registration_count;
    size_t unregistration_count;
} mock_gpio_irq_slot_t;

static mock_gpio_irq_slot_t slots[NUM_BANK0_GPIOS];
static bool dispatcher_initialized;
static bool reject_next_registration;
static bool fire_on_next_registration;
static uint32_t registration_events;

void pico_mock_gpio_irq_reset(void)
{
    for (size_t gpio = 0U; gpio < NUM_BANK0_GPIOS; ++gpio) {
        slots[gpio] = (mock_gpio_irq_slot_t){ 0 };
    }
    dispatcher_initialized = false;
    reject_next_registration = false;
    fire_on_next_registration = false;
    registration_events = 0U;
}

bool platform_gpio_irq_init(void)
{
    dispatcher_initialized = true;
    return true;
}

bool platform_gpio_irq_register(
    unsigned int gpio,
    uint32_t events,
    platform_gpio_irq_handler_t handler,
    void *context)
{
    if (!dispatcher_initialized
            || gpio >= NUM_BANK0_GPIOS
            || events == 0U
            || handler == NULL
            || slots[gpio].handler != NULL
            || reject_next_registration) {
        reject_next_registration = false;
        return false;
    }

    slots[gpio].handler = handler;
    slots[gpio].context = context;
    slots[gpio].events = events;
    slots[gpio].enabled = true;
    slots[gpio].registration_count++;

    if (fire_on_next_registration) {
        fire_on_next_registration = false;
        const uint32_t relevant_events = registration_events & events;
        registration_events = 0U;
        if (relevant_events != 0U) {
            handler(context, gpio, relevant_events);
        }
    }
    return true;
}

bool platform_gpio_irq_unregister(unsigned int gpio)
{
    if (!dispatcher_initialized
            || gpio >= NUM_BANK0_GPIOS
            || slots[gpio].handler == NULL) {
        return false;
    }

    slots[gpio].handler = NULL;
    slots[gpio].context = NULL;
    slots[gpio].events = 0U;
    slots[gpio].enabled = false;
    slots[gpio].unregistration_count++;
    return true;
}

bool platform_gpio_irq_set_enabled(
    unsigned int gpio,
    bool enabled)
{
    if (!dispatcher_initialized
            || gpio >= NUM_BANK0_GPIOS
            || slots[gpio].handler == NULL) {
        return false;
    }

    slots[gpio].enabled = enabled;
    return true;
}

void pico_mock_gpio_irq_reject_next_registration(void)
{
    reject_next_registration = true;
}

void pico_mock_gpio_irq_fire_on_next_registration(uint32_t events)
{
    fire_on_next_registration = true;
    registration_events = events;
}

bool pico_mock_gpio_irq_is_registered(unsigned int gpio)
{
    return gpio < NUM_BANK0_GPIOS && slots[gpio].handler != NULL;
}

bool pico_mock_gpio_irq_is_enabled(unsigned int gpio)
{
    return gpio < NUM_BANK0_GPIOS
        && slots[gpio].handler != NULL
        && slots[gpio].enabled;
}

uint32_t pico_mock_gpio_irq_events(unsigned int gpio)
{
    return gpio < NUM_BANK0_GPIOS ? slots[gpio].events : 0U;
}

size_t pico_mock_gpio_irq_registration_count(unsigned int gpio)
{
    return gpio < NUM_BANK0_GPIOS
        ? slots[gpio].registration_count
        : 0U;
}

size_t pico_mock_gpio_irq_unregistration_count(unsigned int gpio)
{
    return gpio < NUM_BANK0_GPIOS
        ? slots[gpio].unregistration_count
        : 0U;
}

bool pico_mock_gpio_irq_fire(
    unsigned int gpio,
    uint32_t events)
{
    if (gpio >= NUM_BANK0_GPIOS
            || slots[gpio].handler == NULL
            || !slots[gpio].enabled) {
        return false;
    }

    const uint32_t relevant_events = events & slots[gpio].events;
    if (relevant_events == 0U) {
        return false;
    }

    const platform_gpio_irq_handler_t handler = slots[gpio].handler;
    void *const context = slots[gpio].context;
    handler(context, gpio, relevant_events);
    return true;
}
