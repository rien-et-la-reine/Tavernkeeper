#ifndef TAVERNKEEP_TEST_PICO_MOCK_H
#define TAVERNKEEP_TEST_PICO_MOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    PICO_MOCK_MAX_COMMAND_PAYLOAD = 4096,
};

void pico_mock_reset(void);

void pico_mock_gpio_set_input(unsigned int pin, bool value);
bool pico_mock_gpio_set_input_sequence(
    unsigned int pin,
    const bool *values,
    size_t value_count);
size_t pico_mock_gpio_read_count(unsigned int pin);
bool pico_mock_gpio_level(unsigned int pin);
bool pico_mock_gpio_was_pulled_up_at_init(unsigned int pin);
bool pico_mock_gpio_was_initialized(unsigned int pin);
bool pico_mock_gpio_was_deinitialized(unsigned int pin);
unsigned int pico_mock_gpio_function(unsigned int pin);

bool pico_mock_spi_is_initialized(void);
unsigned int pico_mock_spi_initial_baudrate(void);
unsigned int pico_mock_spi_baudrate(void);
size_t pico_mock_spi_transfer_count(void);
const uint8_t *pico_mock_spi_tx_log(void);
size_t pico_mock_sd_pending_response_count(void);

void pico_mock_sd_set_busy_cycles(size_t cycles);
bool pico_mock_sd_set_command(
    uint8_t command,
    uint8_t r1,
    const uint8_t *payload,
    size_t payload_size);
size_t pico_mock_sd_command_count(uint8_t command);
uint32_t pico_mock_sd_last_argument(uint8_t command);

#endif
