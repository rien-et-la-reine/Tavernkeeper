#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "gpio_irq_hardware_mock.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "platform/gpio_irq.h"

enum {
    TEST_GPIO = 6,
};

static int failures;
static size_t callback_count;
static void *observed_context;
static unsigned int observed_gpio;
static uint32_t observed_events;
static bool callback_mutation_ok;

static void self_unregister(void *context, unsigned int gpio, uint32_t events)
{
    (void)context;
    (void)events;
    callback_count++;
    callback_mutation_ok = platform_gpio_irq_unregister(gpio);
}

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            (void)fprintf(stderr, "%s:%d: CHECK failed: %s\n",              \
                __FILE__, __LINE__, #condition);                               \
            failures++;                                                        \
            return 1;                                                          \
        }                                                                      \
    } while (false)

static void test_handler(
    void *context,
    unsigned int gpio,
    uint32_t events)
{
    callback_count++;
    observed_context = context;
    observed_gpio = gpio;
    observed_events = events;
}

int main(void)
{
    int context_value = 42;
    gpio_irq_hardware_mock_reset();

    CHECK(!platform_gpio_irq_register(TEST_GPIO, GPIO_IRQ_EDGE_RISE, test_handler, NULL));
    CHECK(!platform_gpio_irq_unregister(TEST_GPIO));
    CHECK(!platform_gpio_irq_set_enabled(TEST_GPIO, true));
    CHECK(!gpio_irq_hardware_mock_bank_enabled());

    CHECK(platform_gpio_irq_init());
    CHECK(gpio_irq_hardware_mock_callback_set_count() == 1U);
    CHECK(gpio_irq_hardware_mock_bank_enabled());
    CHECK(platform_gpio_irq_init());

    /* The SDK hands the shared callback a raw GPIO index. The dispatcher owns
     * one slot per bank-0 pin, so an index at or past the end must be dropped
     * before it is used to subscript the table. Delivering the boundary index
     * itself matters: a bounds check written with > instead of >= reads one
     * past the array, which is a silent out-of-bounds read in a release build
     * and an AddressSanitizer report in a diagnostic one. */
    for (unsigned int offset = 0U; offset < 3U; ++offset) {
        gpio_irq_hardware_mock_deliver(
            NUM_BANK0_GPIOS + offset, GPIO_IRQ_EDGE_RISE);
        CHECK(callback_count == 0U);
    }

    CHECK(!platform_gpio_irq_register(
        NUM_BANK0_GPIOS,
        GPIO_IRQ_EDGE_RISE,
        test_handler,
        &context_value));
    CHECK(!platform_gpio_irq_register(
        TEST_GPIO,
        0U,
        test_handler,
        &context_value));
    CHECK(!platform_gpio_irq_register(
        TEST_GPIO,
        0x10U,
        test_handler,
        &context_value));
    CHECK(!platform_gpio_irq_register(
        TEST_GPIO,
        GPIO_IRQ_EDGE_RISE,
        NULL,
        &context_value));

    CHECK(platform_gpio_irq_register(
        TEST_GPIO,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        test_handler,
        &context_value));
    CHECK(gpio_irq_hardware_mock_pin_enabled(
        TEST_GPIO,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL));

    CHECK(!platform_gpio_irq_register(
        TEST_GPIO,
        GPIO_IRQ_EDGE_RISE,
        test_handler,
        &context_value));

    CHECK(gpio_irq_hardware_mock_fire(TEST_GPIO, GPIO_IRQ_EDGE_RISE));
    CHECK(callback_count == 1U);
    CHECK(observed_context == &context_value);
    CHECK(observed_gpio == TEST_GPIO);
    CHECK(observed_events == GPIO_IRQ_EDGE_RISE);

    CHECK(platform_gpio_irq_set_enabled(TEST_GPIO, false));
    CHECK(!gpio_irq_hardware_mock_pin_enabled(
        TEST_GPIO,
        GPIO_IRQ_EDGE_RISE));
    CHECK(!gpio_irq_hardware_mock_fire(TEST_GPIO, GPIO_IRQ_EDGE_RISE));
    CHECK(callback_count == 1U);

    CHECK(platform_gpio_irq_set_enabled(TEST_GPIO, true));
    CHECK(gpio_irq_hardware_mock_fire(TEST_GPIO, GPIO_IRQ_EDGE_FALL));
    CHECK(callback_count == 2U);
    CHECK(observed_events == GPIO_IRQ_EDGE_FALL);

    gpio_irq_hardware_mock_set_core(1U);
    CHECK(!platform_gpio_irq_register(TEST_GPIO + 1U, GPIO_IRQ_EDGE_RISE, test_handler, NULL));
    CHECK(!platform_gpio_irq_init());
    CHECK(!platform_gpio_irq_set_enabled(TEST_GPIO, false));
    CHECK(!platform_gpio_irq_unregister(TEST_GPIO));

    gpio_irq_hardware_mock_set_core(0U);
    CHECK(platform_gpio_irq_unregister(TEST_GPIO));
    CHECK(!gpio_irq_hardware_mock_pin_enabled(
        TEST_GPIO,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL));
    CHECK(!platform_gpio_irq_unregister(TEST_GPIO));
    CHECK(!gpio_irq_hardware_mock_fire(TEST_GPIO, GPIO_IRQ_EDGE_RISE));

    CHECK(platform_gpio_irq_register(
        TEST_GPIO,
        GPIO_IRQ_EDGE_RISE,
        test_handler,
        &context_value));

    /* A second peripheral may subscribe without replacing the SDK callback. */
    CHECK(platform_gpio_irq_register(NUM_BANK0_GPIOS - 1U,
        GPIO_IRQ_LEVEL_LOW | GPIO_IRQ_LEVEL_HIGH, test_handler, NULL));
    CHECK(platform_gpio_irq_init());
    CHECK(gpio_irq_hardware_mock_callback_set_count() == 1U);
    size_t before = callback_count;
    gpio_irq_hardware_mock_deliver(TEST_GPIO,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL | GPIO_IRQ_LEVEL_HIGH);
    CHECK(callback_count == before + 1U && observed_events == GPIO_IRQ_EDGE_RISE);
    gpio_irq_hardware_mock_deliver(TEST_GPIO, GPIO_IRQ_EDGE_FALL);
    gpio_irq_hardware_mock_deliver(NUM_BANK0_GPIOS, GPIO_IRQ_EDGE_RISE);
    gpio_irq_hardware_mock_deliver(TEST_GPIO + 1U, GPIO_IRQ_EDGE_RISE);
    CHECK(callback_count == before + 1U);
    CHECK(gpio_irq_hardware_mock_fire(NUM_BANK0_GPIOS - 1U, GPIO_IRQ_LEVEL_HIGH));
    CHECK(observed_context == NULL && observed_gpio == NUM_BANK0_GPIOS - 1U);
    CHECK(platform_gpio_irq_unregister(TEST_GPIO));
    CHECK(gpio_irq_hardware_mock_fire(NUM_BANK0_GPIOS - 1U, GPIO_IRQ_LEVEL_LOW));
    CHECK(observed_events == GPIO_IRQ_LEVEL_LOW);
    CHECK(!platform_gpio_irq_unregister(NUM_BANK0_GPIOS));
    CHECK(!platform_gpio_irq_set_enabled(NUM_BANK0_GPIOS, true));
    CHECK(!platform_gpio_irq_set_enabled(TEST_GPIO, true));

    /* Nested critical sections must preserve the caller's disabled state,
     * including duplicate registration and missing-owner failure paths. */
    uint32_t saved = save_and_disable_interrupts();
    CHECK(platform_gpio_irq_init());
    CHECK(platform_gpio_irq_register(TEST_GPIO, GPIO_IRQ_EDGE_RISE, self_unregister, NULL));
    CHECK(!platform_gpio_irq_register(TEST_GPIO, GPIO_IRQ_EDGE_FALL, test_handler, NULL));
    CHECK(platform_gpio_irq_set_enabled(TEST_GPIO, false));
    CHECK(platform_gpio_irq_set_enabled(TEST_GPIO, true));
    CHECK(!platform_gpio_irq_unregister(TEST_GPIO + 1U));
    CHECK(!platform_gpio_irq_set_enabled(TEST_GPIO + 1U, false));
    CHECK(!gpio_irq_hardware_mock_interrupts_enabled());
    restore_interrupts(saved);
    CHECK(gpio_irq_hardware_mock_interrupts_enabled());
    CHECK(gpio_irq_hardware_mock_fire(TEST_GPIO, GPIO_IRQ_EDGE_RISE));
    CHECK(callback_mutation_ok);
    CHECK(!gpio_irq_hardware_mock_fire(TEST_GPIO, GPIO_IRQ_EDGE_RISE));
    CHECK(platform_gpio_irq_unregister(NUM_BANK0_GPIOS - 1U));

    if (failures != 0) {
        return 1;
    }

    (void)printf("PASS GPIO IRQ dispatcher lifecycle and routing\n");
    return 0;
}
