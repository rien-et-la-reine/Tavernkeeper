#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico_mock.h"
#include "storage/sd_spi.h"

enum {
    PIN_CLOCK = 2,
    PIN_CONTROLLER_OUT = 3,
    PIN_CONTROLLER_IN = 4,
    PIN_CHIP_SELECT = 5,
    PIN_CARD_AVAILABLE = 6,
    OPERATIONAL_BAUDRATE = 12000000,
};

static int failures;
static int tests_run;
static spi_inst_t test_spi = { .id = 0U };

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            (void)fprintf(stderr, "%s:%d: CHECK failed: %s\n",                \
                __FILE__, __LINE__, #condition);                               \
            failures++;                                                        \
            return;                                                            \
        }                                                                      \
    } while (false)

#define CHECK_EQ(expected, actual) CHECK((expected) == (actual))

static sd_spi_config_t valid_config(void)
{
    const sd_spi_config_t config = {
        .spi = &test_spi,
        .baud_rate_hz = OPERATIONAL_BAUDRATE,
        .pin_clock = PIN_CLOCK,
        .pin_controller_out = PIN_CONTROLLER_OUT,
        .pin_controller_in = PIN_CONTROLLER_IN,
        .pin_chip_select = PIN_CHIP_SELECT,
        .pin_card_available = PIN_CARD_AVAILABLE,
    };
    return config;
}

static void configure_successful_sdhc_card(void)
{
    static const uint8_t r7[] = { 0x00U, 0x00U, 0x01U, 0xaaU };
    static const uint8_t ocr[] = { 0xc0U, 0xffU, 0x80U, 0x00U };

    pico_mock_sd_set_command(0U, 0x01U, NULL, 0U);
    pico_mock_sd_set_command(8U, 0x01U, r7, sizeof(r7));
    pico_mock_sd_set_command(55U, 0x01U, NULL, 0U);
    pico_mock_sd_set_command(41U, 0x00U, NULL, 0U);
    pico_mock_sd_set_command(58U, 0x00U, ocr, sizeof(ocr));
}

static block_device_t *initialize_sdhc(sd_spi_t *sd)
{
    const sd_spi_config_t config = valid_config();
    if (sd_spi_configure(sd, &config) != BLOCK_DEVICE_RESULT_OK) {
        (void)fprintf(stderr, "%s:%d: SDHC configure failed\n",
            __FILE__, __LINE__);
        failures++;
        return NULL;
    }
    configure_successful_sdhc_card();
    block_device_t *const device = sd_spi_as_block_device(sd);
    if (device == NULL
            || block_device_init(device) != BLOCK_DEVICE_RESULT_OK) {
        (void)fprintf(stderr, "%s:%d: SDHC initialization failed\n",
            __FILE__, __LINE__);
        failures++;
        return NULL;
    }
    return device;
}

static void test_configure_validation(void)
{
    pico_mock_reset();
    sd_spi_t sd = { 0 };
    sd_spi_config_t config = valid_config();

    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        sd_spi_configure(NULL, &config));
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        sd_spi_configure(&sd, NULL));
    config.spi = NULL;
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        sd_spi_configure(&sd, &config));
    config = valid_config();
    config.baud_rate_hz = 0U;
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        sd_spi_configure(&sd, &config));
    config.baud_rate_hz = 25000001U;
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        sd_spi_configure(&sd, &config));
}

static void test_configure_exposes_uninitialized_device(void)
{
    pico_mock_reset();
    sd_spi_t sd = { 0 };
    const sd_spi_config_t config = valid_config();
    uint8_t block[512] = { 0 };
    block_device_info_t info = { 0 };

    CHECK(sd_spi_as_block_device(NULL) == NULL);
    CHECK(sd_spi_as_block_device(&sd) == NULL);
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    block_device_t *const device = sd_spi_as_block_device(&sd);
    CHECK(device != NULL);
    CHECK(block_device_is_valid(device));
    CHECK_EQ(BLOCK_DEVICE_RESULT_NOT_INITIALIZED,
        block_device_read_blocks(device, 0U, block, 1U));
    CHECK_EQ(BLOCK_DEVICE_RESULT_NOT_INITIALIZED,
        block_device_write_blocks(device, 0U, block, 1U));
    CHECK_EQ(BLOCK_DEVICE_RESULT_NOT_INITIALIZED,
        block_device_get_info(device, &info));
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        block_device_read_blocks(device, 0U, NULL, 1U));
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        block_device_write_blocks(device, 0U, block, 0U));
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        block_device_get_info(device, NULL));
}

static void test_sdhc_initialization(void)
{
    pico_mock_reset();
    sd_spi_t sd = { 0 };
    (void)initialize_sdhc(&sd);

    CHECK(sd.initialized);
    CHECK(!sd.card_type_legacy);
    CHECK(sd.card_type_hcxc);
    CHECK(pico_mock_spi_is_initialized());
    CHECK_EQ(400000U, pico_mock_spi_initial_baudrate());
    CHECK_EQ(OPERATIONAL_BAUDRATE, pico_mock_spi_baudrate());
    CHECK(pico_mock_gpio_was_initialized(PIN_CHIP_SELECT));
    CHECK(pico_mock_gpio_was_initialized(PIN_CARD_AVAILABLE));
    CHECK_EQ(GPIO_FUNC_SPI, pico_mock_gpio_function(PIN_CLOCK));
    CHECK_EQ(GPIO_FUNC_SPI, pico_mock_gpio_function(PIN_CONTROLLER_OUT));
    CHECK_EQ(GPIO_FUNC_SPI, pico_mock_gpio_function(PIN_CONTROLLER_IN));
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));
    CHECK_EQ(1U, pico_mock_sd_command_count(0U));
    CHECK_EQ(1U, pico_mock_sd_command_count(8U));
    CHECK_EQ(1U, pico_mock_sd_command_count(55U));
    CHECK_EQ(1U, pico_mock_sd_command_count(41U));
    CHECK_EQ(0x40000000U, pico_mock_sd_last_argument(41U));
    CHECK_EQ(1U, pico_mock_sd_command_count(58U));
    CHECK_EQ(0U, pico_mock_sd_command_count(16U));
}

static void test_legacy_card_initialization(void)
{
    pico_mock_reset();
    sd_spi_t sd = { 0 };
    const sd_spi_config_t config = valid_config();
    static const uint8_t ocr[] = { 0x80U, 0x00U, 0x00U, 0x00U };

    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    pico_mock_sd_set_command(0U, 0x01U, NULL, 0U);
    pico_mock_sd_set_command(8U, 0x05U, NULL, 0U);
    pico_mock_sd_set_command(55U, 0x01U, NULL, 0U);
    pico_mock_sd_set_command(41U, 0x00U, NULL, 0U);
    pico_mock_sd_set_command(58U, 0x00U, ocr, sizeof(ocr));
    pico_mock_sd_set_command(16U, 0x00U, NULL, 0U);

    CHECK_EQ(BLOCK_DEVICE_RESULT_OK,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK(sd.initialized);
    CHECK(sd.card_type_legacy);
    CHECK(!sd.card_type_hcxc);
    CHECK_EQ(0U, pico_mock_sd_last_argument(41U));
    CHECK_EQ(1U, pico_mock_sd_command_count(16U));
    CHECK_EQ(512U, pico_mock_sd_last_argument(16U));
}

static void test_missing_card_fails_initialization(void)
{
    pico_mock_reset();
    sd_spi_t sd = { 0 };
    const sd_spi_config_t config = valid_config();

    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    pico_mock_gpio_set_input(PIN_CARD_AVAILABLE, true);
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_DEVICE,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK(!sd.initialized);
    CHECK_EQ(0U, pico_mock_sd_command_count(0U));
}

static void test_card_initialization_timeout_is_bounded(void)
{
    pico_mock_reset();
    sd_spi_t sd = { 0 };
    const sd_spi_config_t config = valid_config();
    static const uint8_t r7[] = { 0x00U, 0x00U, 0x01U, 0xaaU };

    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    pico_mock_sd_set_command(0U, 0x01U, NULL, 0U);
    pico_mock_sd_set_command(8U, 0x01U, r7, sizeof(r7));
    pico_mock_sd_set_command(55U, 0x01U, NULL, 0U);
    pico_mock_sd_set_command(41U, 0x01U, NULL, 0U);

    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK(!sd.initialized);
    CHECK(pico_mock_sd_command_count(41U) > 1U);
    CHECK(pico_mock_sd_command_count(41U) < 1200U);
    CHECK(pico_mock_spi_transfer_count() < 16384U);
}

static void test_deinit_waits_and_releases_resources(void)
{
    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *const device = initialize_sdhc(&sd);

    pico_mock_sd_set_busy_cycles(3U);
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, block_device_deinit(device));
    CHECK(!sd.initialized);
    CHECK(!sd.card_type_legacy);
    CHECK(!sd.card_type_hcxc);
    CHECK(!pico_mock_spi_is_initialized());
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));
    CHECK(pico_mock_gpio_was_deinitialized(PIN_CLOCK));
    CHECK(pico_mock_gpio_was_deinitialized(PIN_CONTROLLER_OUT));
    CHECK(pico_mock_gpio_was_deinitialized(PIN_CONTROLLER_IN));
    CHECK(pico_mock_gpio_was_deinitialized(PIN_CHIP_SELECT));
    CHECK(pico_mock_gpio_was_deinitialized(PIN_CARD_AVAILABLE));
}

static void test_deinit_timeout_preserves_resources(void)
{
    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *const device = initialize_sdhc(&sd);

    pico_mock_sd_set_busy_cycles(2000U);
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR, block_device_deinit(device));
    CHECK(sd.initialized);
    CHECK(pico_mock_spi_is_initialized());
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));
    CHECK(!pico_mock_gpio_was_deinitialized(PIN_CLOCK));
    CHECK(!pico_mock_gpio_was_deinitialized(PIN_CHIP_SELECT));
}

static void test_deinit_before_init_does_not_transfer(void)
{
    pico_mock_reset();
    sd_spi_t sd = { 0 };
    const sd_spi_config_t config = valid_config();

    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK,
        block_device_deinit(sd_spi_as_block_device(&sd)));
    CHECK_EQ(0U, pico_mock_spi_transfer_count());
    CHECK(!pico_mock_spi_is_initialized());
}

static void run_test(void (*test)(void), const char *name)
{
    const int failures_before = failures;
    tests_run++;
    test();
    if (failures == failures_before) {
        (void)printf("PASS %s\n", name);
    } else {
        (void)printf("FAIL %s\n", name);
    }
}

int main(void)
{
    run_test(test_configure_validation, "configure validation");
    run_test(test_configure_exposes_uninitialized_device,
        "configured device preconditions");
    run_test(test_sdhc_initialization, "SDHC initialization");
    run_test(test_legacy_card_initialization, "legacy card initialization");
    run_test(test_missing_card_fails_initialization,
        "missing card initialization failure");
    run_test(test_card_initialization_timeout_is_bounded,
        "card initialization timeout");
    run_test(test_deinit_waits_and_releases_resources,
        "deinit waits and releases resources");
    run_test(test_deinit_timeout_preserves_resources,
        "deinit timeout preserves resources");
    run_test(test_deinit_before_init_does_not_transfer,
        "deinit before init");

    if (failures != 0) {
        (void)fprintf(stderr, "%d of %d tests failed\n", failures, tests_run);
        return 1;
    }

    (void)printf("All %d tests passed\n", tests_run);
    return 0;
}
