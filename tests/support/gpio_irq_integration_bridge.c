/* The integration executable starts one fresh dispatcher per process. Only
 * SDK hardware is faked: registration, dispatch and removal run production code. */
#include "gpio_irq_hardware_mock.h"
#include "pico_mock.h"
void pico_mock_gpio_irq_reset(void) { gpio_irq_hardware_mock_reset(); }
bool pico_mock_gpio_irq_fire(unsigned int gpio, uint32_t events)
{
    return gpio_irq_hardware_mock_fire(gpio, events);
}
