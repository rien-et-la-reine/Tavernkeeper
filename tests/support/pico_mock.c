#include "pico_mock.h"

#include <string.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/time.h"

enum {
    MOCK_PIN_COUNT = 64,
    MOCK_COMMAND_COUNT = 64,
    MOCK_RESPONSE_CAPACITY = 8192,
    MOCK_TX_CAPACITY = 16384,
};

typedef struct {
    bool configured;
    uint8_t r1;
    uint8_t payload[PICO_MOCK_MAX_COMMAND_PAYLOAD];
    size_t payload_size;
    size_t count;
    uint32_t last_argument;
} mock_command_t;

static bool gpio_levels[MOCK_PIN_COUNT];
static bool gpio_initialized[MOCK_PIN_COUNT];
static bool gpio_deinitialized[MOCK_PIN_COUNT];
static bool gpio_directions[MOCK_PIN_COUNT];
static unsigned int gpio_functions[MOCK_PIN_COUNT];

static bool spi_initialized;
static unsigned int spi_initial_baudrate;
static unsigned int spi_baudrate;
static uint8_t spi_tx_log[MOCK_TX_CAPACITY];
static size_t spi_tx_count;

static mock_command_t commands[MOCK_COMMAND_COUNT];
static uint8_t command_bytes[6];
static size_t command_byte_count;
static size_t busy_cycles;
static uint8_t response_bytes[MOCK_RESPONSE_CAPACITY];
static size_t response_head;
static size_t response_tail;
static absolute_time_t mock_now;

static bool valid_pin(unsigned int pin)
{
    return pin < MOCK_PIN_COUNT;
}

static void enqueue_response(uint8_t value)
{
    if (response_tail < MOCK_RESPONSE_CAPACITY) {
        response_bytes[response_tail++] = value;
    }
}

static uint8_t complete_command(void)
{
    const uint8_t command = command_bytes[0] & 0x3fU;
    command_byte_count = 0U;

    /* CMD12 aborts any unread data from an active multiple-block stream. */
    if (command == 12U) {
        response_head = 0U;
        response_tail = 0U;
    }

    if (command >= MOCK_COMMAND_COUNT) {
        return 0xffU;
    }

    mock_command_t *const behavior = &commands[command];
    behavior->count++;
    behavior->last_argument =
        ((uint32_t)command_bytes[1] << 24U)
        | ((uint32_t)command_bytes[2] << 16U)
        | ((uint32_t)command_bytes[3] << 8U)
        | (uint32_t)command_bytes[4];

    if (!behavior->configured) {
        enqueue_response(0xffU);
        return 0xffU;
    }

    if (command == 12U) {
        /* CMD12 has one positional stuff byte before its R1 response. */
        enqueue_response(0x00U);
    }
    enqueue_response(behavior->r1);
    for (size_t i = 0U; i < behavior->payload_size; ++i) {
        enqueue_response(behavior->payload[i]);
    }
    return 0xffU;
}

static uint8_t transfer_byte(uint8_t tx)
{
    if (spi_tx_count < MOCK_TX_CAPACITY) {
        spi_tx_log[spi_tx_count++] = tx;
    }

    if (command_byte_count != 0U) {
        command_bytes[command_byte_count++] = tx;
        if (command_byte_count == sizeof(command_bytes)) {
            return complete_command();
        }
        return 0xffU;
    }

    if ((tx & 0xc0U) == 0x40U) {
        command_bytes[0] = tx;
        command_byte_count = 1U;
        return 0xffU;
    }

    if (response_head < response_tail) {
        return response_bytes[response_head++];
    }

    response_head = 0U;
    response_tail = 0U;

    if (tx == 0xffU && busy_cycles != 0U) {
        busy_cycles--;
        return 0x00U;
    }

    return 0xffU;
}

void pico_mock_reset(void)
{
    memset(gpio_levels, 0, sizeof(gpio_levels));
    memset(gpio_initialized, 0, sizeof(gpio_initialized));
    memset(gpio_deinitialized, 0, sizeof(gpio_deinitialized));
    memset(gpio_directions, 0, sizeof(gpio_directions));
    memset(gpio_functions, 0, sizeof(gpio_functions));
    spi_initialized = false;
    spi_initial_baudrate = 0U;
    spi_baudrate = 0U;
    memset(spi_tx_log, 0, sizeof(spi_tx_log));
    spi_tx_count = 0U;
    memset(commands, 0, sizeof(commands));
    memset(command_bytes, 0, sizeof(command_bytes));
    command_byte_count = 0U;
    busy_cycles = 0U;
    memset(response_bytes, 0, sizeof(response_bytes));
    response_head = 0U;
    response_tail = 0U;
    mock_now = 0U;
}

void pico_mock_gpio_set_input(unsigned int pin, bool value)
{
    if (valid_pin(pin)) {
        gpio_levels[pin] = value;
    }
}

bool pico_mock_gpio_level(unsigned int pin)
{
    return valid_pin(pin) && gpio_levels[pin];
}

bool pico_mock_gpio_was_initialized(unsigned int pin)
{
    return valid_pin(pin) && gpio_initialized[pin];
}

bool pico_mock_gpio_was_deinitialized(unsigned int pin)
{
    return valid_pin(pin) && gpio_deinitialized[pin];
}

unsigned int pico_mock_gpio_function(unsigned int pin)
{
    return valid_pin(pin) ? gpio_functions[pin] : 0U;
}

bool pico_mock_spi_is_initialized(void)
{
    return spi_initialized;
}

unsigned int pico_mock_spi_initial_baudrate(void)
{
    return spi_initial_baudrate;
}

unsigned int pico_mock_spi_baudrate(void)
{
    return spi_baudrate;
}

size_t pico_mock_spi_transfer_count(void)
{
    return spi_tx_count;
}

const uint8_t *pico_mock_spi_tx_log(void)
{
    return spi_tx_log;
}

size_t pico_mock_sd_pending_response_count(void)
{
    return response_tail - response_head;
}

void pico_mock_sd_set_busy_cycles(size_t cycles)
{
    busy_cycles = cycles;
}

bool pico_mock_sd_set_command(
    uint8_t command,
    uint8_t r1,
    const uint8_t *payload,
    size_t payload_size)
{
    if (command >= MOCK_COMMAND_COUNT
            || payload_size > PICO_MOCK_MAX_COMMAND_PAYLOAD
            || (payload_size != 0U && payload == NULL)) {
        return false;
    }

    mock_command_t *const behavior = &commands[command];
    behavior->configured = true;
    behavior->r1 = r1;
    behavior->payload_size = payload_size;
    if (payload_size != 0U) {
        memcpy(behavior->payload, payload, payload_size);
    }
    return true;
}

size_t pico_mock_sd_command_count(uint8_t command)
{
    return command < MOCK_COMMAND_COUNT ? commands[command].count : 0U;
}

uint32_t pico_mock_sd_last_argument(uint8_t command)
{
    return command < MOCK_COMMAND_COUNT
        ? commands[command].last_argument
        : 0U;
}

void gpio_init(unsigned int pin)
{
    if (valid_pin(pin)) {
        gpio_initialized[pin] = true;
        gpio_deinitialized[pin] = false;
    }
}

void gpio_deinit(unsigned int pin)
{
    if (valid_pin(pin)) {
        gpio_deinitialized[pin] = true;
        gpio_initialized[pin] = false;
    }
}

void gpio_put(unsigned int pin, bool value)
{
    if (valid_pin(pin)) {
        gpio_levels[pin] = value;
    }
}

bool gpio_get(unsigned int pin)
{
    return valid_pin(pin) && gpio_levels[pin];
}

void gpio_set_dir(unsigned int pin, bool out)
{
    if (valid_pin(pin)) {
        gpio_directions[pin] = out;
    }
}

void gpio_set_function(unsigned int pin, unsigned int function)
{
    if (valid_pin(pin)) {
        gpio_functions[pin] = function;
    }
}

unsigned int spi_init(spi_inst_t *spi, unsigned int baudrate)
{
    (void)spi;
    spi_initialized = true;
    spi_initial_baudrate = baudrate;
    spi_baudrate = baudrate;
    return baudrate;
}

void spi_deinit(spi_inst_t *spi)
{
    (void)spi;
    spi_initialized = false;
}

unsigned int spi_set_baudrate(spi_inst_t *spi, unsigned int baudrate)
{
    (void)spi;
    spi_baudrate = baudrate;
    return baudrate;
}

int spi_write_read_blocking(
    spi_inst_t *spi,
    const uint8_t *source,
    uint8_t *destination,
    size_t length)
{
    (void)spi;
    for (size_t i = 0U; i < length; ++i) {
        destination[i] = transfer_byte(source[i]);
    }
    return (int)length;
}

absolute_time_t make_timeout_time_ms(uint32_t milliseconds)
{
    return mock_now + milliseconds;
}

bool time_reached(absolute_time_t target)
{
    const bool reached = mock_now >= target;
    mock_now++;
    return reached;
}

void sleep_ms(uint32_t milliseconds)
{
    mock_now += milliseconds;
}
