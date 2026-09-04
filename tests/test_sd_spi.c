#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
    SD_BLOCK_SIZE = 512,
    SDHC_C_SIZE = 4095,
    SDHC_BLOCK_COUNT = (SDHC_C_SIZE + 1) * 1024,
    SDSC_C_SIZE = 1023,
    SDSC_C_SIZE_MULT = 7,
    SDSC_BLOCK_COUNT = (SDSC_C_SIZE + 1) * (1 << (SDSC_C_SIZE_MULT + 2)),
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

static void configure_csd_v2_fields(uint32_t c_size)
{
    uint8_t payload[19] = { 0 };
    payload[0] = 0xfeU;
    payload[1] = 0x40U;
    payload[8] = (uint8_t)((c_size >> 16U) & 0x3fU);
    payload[9] = (uint8_t)(c_size >> 8U);
    payload[10] = (uint8_t)c_size;
    pico_mock_sd_set_command(9U, 0x00U, payload, sizeof(payload));
}

static void configure_csd_v2(void)
{
    configure_csd_v2_fields(SDHC_C_SIZE);
}

static void configure_csd_v1_fields(
    uint32_t c_size,
    uint8_t c_size_mult,
    uint8_t read_bl_len)
{
    uint8_t payload[19] = { 0 };
    payload[0] = 0xfeU;
    payload[6] = read_bl_len & 0x0fU;
    payload[7] = (uint8_t)((c_size >> 10U) & 0x03U);
    payload[8] = (uint8_t)(c_size >> 2U);
    payload[9] = (uint8_t)((c_size & 0x03U) << 6U);
    payload[10] = (uint8_t)((c_size_mult >> 1U) & 0x03U);
    payload[11] = (uint8_t)((c_size_mult & 0x01U) << 7U);
    pico_mock_sd_set_command(9U, 0x00U, payload, sizeof(payload));
}

static void configure_csd_v1(void)
{
    configure_csd_v1_fields(SDSC_C_SIZE, SDSC_C_SIZE_MULT, 9U);
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
    configure_csd_v2();
}

static void configure_successful_sdsc_card(void)
{
    static const uint8_t ocr[] = { 0x80U, 0x00U, 0x00U, 0x00U };

    pico_mock_sd_set_command(0U, 0x01U, NULL, 0U);
    pico_mock_sd_set_command(8U, 0x05U, NULL, 0U);
    pico_mock_sd_set_command(55U, 0x01U, NULL, 0U);
    pico_mock_sd_set_command(41U, 0x00U, NULL, 0U);
    pico_mock_sd_set_command(58U, 0x00U, ocr, sizeof(ocr));
    configure_csd_v1();
    pico_mock_sd_set_command(16U, 0x00U, NULL, 0U);
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

static block_device_t *initialize_legacy(sd_spi_t *sd)
{
    const sd_spi_config_t config = valid_config();

    if (sd_spi_configure(sd, &config) != BLOCK_DEVICE_RESULT_OK) {
        failures++;
        return NULL;
    }
    configure_successful_sdsc_card();

    block_device_t *const device = sd_spi_as_block_device(sd);
    if (device == NULL
            || block_device_init(device) != BLOCK_DEVICE_RESULT_OK) {
        failures++;
        return NULL;
    }
    return device;
}

static uint8_t expected_data_byte(
    uint8_t seed,
    size_t block,
    size_t offset)
{
    return (uint8_t)(seed + (uint8_t)(block * 17U) + (uint8_t)offset);
}

static size_t make_read_payload(
    uint8_t *payload,
    size_t block_count,
    uint8_t seed)
{
    size_t position = 0U;
    for (size_t block = 0U; block < block_count; ++block) {
        payload[position++] = 0xfeU;
        for (size_t offset = 0U; offset < SD_BLOCK_SIZE; ++offset) {
            payload[position++] = expected_data_byte(seed, block, offset);
        }
        payload[position++] = 0x12U;
        payload[position++] = 0x34U;
    }
    return position;
}

static bool buffer_matches(
    const uint8_t *buffer,
    size_t block_count,
    uint8_t seed)
{
    for (size_t block = 0U; block < block_count; ++block) {
        for (size_t offset = 0U; offset < SD_BLOCK_SIZE; ++offset) {
            if (buffer[(block * SD_BLOCK_SIZE) + offset]
                    != expected_data_byte(seed, block, offset)) {
                return false;
            }
        }
    }
    return true;
}

typedef struct {
    block_device_result_t result;
    size_t transfer_count;
    size_t cmd9_count;
    size_t cmd12_count;
    size_t cmd17_count;
    size_t cmd18_count;
    bool chip_select_released;
    bool initialized;
    bool card_type_legacy;
    bool card_type_hcxc;
    uint64_t block_count;
    bool spi_initialized;
    bool all_gpio_deinitialized;
} error_token_observation_t;

static error_token_observation_t observe_cmd17_error_token(uint8_t token)
{
    uint8_t block[SD_BLOCK_SIZE];
    error_token_observation_t observation = { 0 };

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *const device = initialize_sdhc(&sd);
    if (device == NULL
            || !pico_mock_sd_set_command(17U, 0x00U, &token, 1U)) {
        failures++;
        observation.result = BLOCK_DEVICE_RESULT_INVALID_ARGUMENT;
        return observation;
    }

    const size_t transfers_before = pico_mock_spi_transfer_count();
    observation.result = block_device_read_blocks(device, 0U, block, 1U);
    observation.transfer_count =
        pico_mock_spi_transfer_count() - transfers_before;
    observation.cmd17_count = pico_mock_sd_command_count(17U);
    observation.chip_select_released =
        pico_mock_gpio_level(PIN_CHIP_SELECT);
    return observation;
}

static error_token_observation_t observe_cmd18_error_token(uint8_t token)
{
    uint8_t blocks[2U * SD_BLOCK_SIZE];
    error_token_observation_t observation = { 0 };

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *const device = initialize_sdhc(&sd);
    if (device == NULL
            || !pico_mock_sd_set_command(18U, 0x00U, &token, 1U)
            || !pico_mock_sd_set_command(12U, 0x00U, NULL, 0U)) {
        failures++;
        observation.result = BLOCK_DEVICE_RESULT_INVALID_ARGUMENT;
        return observation;
    }

    const size_t transfers_before = pico_mock_spi_transfer_count();
    observation.result = block_device_read_blocks(device, 0U, blocks, 2U);
    observation.transfer_count =
        pico_mock_spi_transfer_count() - transfers_before;
    observation.cmd12_count = pico_mock_sd_command_count(12U);
    observation.cmd18_count = pico_mock_sd_command_count(18U);
    observation.chip_select_released =
        pico_mock_gpio_level(PIN_CHIP_SELECT);
    return observation;
}

static error_token_observation_t observe_cmd9_error_token(uint8_t token)
{
    error_token_observation_t observation = { 0 };
    const sd_spi_config_t config = valid_config();

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    if (sd_spi_configure(&sd, &config) != BLOCK_DEVICE_RESULT_OK) {
        failures++;
        observation.result = BLOCK_DEVICE_RESULT_INVALID_ARGUMENT;
        return observation;
    }
    configure_successful_sdhc_card();
    if (!pico_mock_sd_set_command(9U, 0x00U, &token, 1U)) {
        failures++;
        observation.result = BLOCK_DEVICE_RESULT_INVALID_ARGUMENT;
        return observation;
    }

    observation.result = block_device_init(sd_spi_as_block_device(&sd));
    observation.transfer_count = pico_mock_spi_transfer_count();
    observation.cmd9_count = pico_mock_sd_command_count(9U);
    observation.chip_select_released =
        pico_mock_gpio_level(PIN_CHIP_SELECT);
    observation.initialized = sd.initialized;
    observation.card_type_legacy = sd.card_type_legacy;
    observation.card_type_hcxc = sd.card_type_hcxc;
    observation.block_count = sd.block_count;
    observation.spi_initialized = pico_mock_spi_is_initialized();
    observation.all_gpio_deinitialized =
        pico_mock_gpio_was_deinitialized(PIN_CLOCK)
        && pico_mock_gpio_was_deinitialized(PIN_CONTROLLER_OUT)
        && pico_mock_gpio_was_deinitialized(PIN_CONTROLLER_IN)
        && pico_mock_gpio_was_deinitialized(PIN_CHIP_SELECT)
        && pico_mock_gpio_was_deinitialized(PIN_CARD_AVAILABLE);
    return observation;
}

static void verify_data_error_token(uint8_t token)
{
    const error_token_observation_t cmd17_baseline =
        observe_cmd17_error_token(0x01U);
    const error_token_observation_t cmd17 = token == 0x01U
        ? cmd17_baseline : observe_cmd17_error_token(token);
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR, cmd17.result);
    CHECK_EQ(cmd17_baseline.transfer_count, cmd17.transfer_count);
    CHECK_EQ(1U, cmd17.cmd17_count);
    CHECK(cmd17.chip_select_released);

    const error_token_observation_t cmd18_baseline =
        observe_cmd18_error_token(0x01U);
    const error_token_observation_t cmd18 = token == 0x01U
        ? cmd18_baseline : observe_cmd18_error_token(token);
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR, cmd18.result);
    CHECK_EQ(cmd18_baseline.transfer_count, cmd18.transfer_count);
    CHECK_EQ(1U, cmd18.cmd18_count);
    CHECK_EQ(1U, cmd18.cmd12_count);
    CHECK(cmd18.chip_select_released);

    const error_token_observation_t cmd9_baseline =
        observe_cmd9_error_token(0x01U);
    const error_token_observation_t cmd9 = token == 0x01U
        ? cmd9_baseline : observe_cmd9_error_token(token);
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR, cmd9.result);
    CHECK_EQ(cmd9_baseline.transfer_count, cmd9.transfer_count);
    CHECK_EQ(1U, cmd9.cmd9_count);
    CHECK(cmd9.chip_select_released);
    CHECK(!cmd9.initialized);
    CHECK(!cmd9.card_type_legacy);
    CHECK(!cmd9.card_type_hcxc);
    CHECK_EQ(0U, cmd9.block_count);
    CHECK(!cmd9.spi_initialized);
    CHECK(cmd9.all_gpio_deinitialized);
}

static void test_data_error_token_01_is_immediate(void)
{
    verify_data_error_token(0x01U);
}

static void test_data_error_token_02_is_immediate(void)
{
    verify_data_error_token(0x02U);
}

static void test_data_error_token_04_is_immediate(void)
{
    verify_data_error_token(0x04U);
}

static void test_data_error_token_08_is_immediate(void)
{
    verify_data_error_token(0x08U);
}

static void test_zero_is_not_a_data_error_token(void)
{
    const error_token_observation_t cmd17_error =
        observe_cmd17_error_token(0x01U);
    const error_token_observation_t cmd17_zero =
        observe_cmd17_error_token(0x00U);
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR, cmd17_zero.result);
    CHECK(cmd17_zero.transfer_count > cmd17_error.transfer_count);
    CHECK(cmd17_zero.chip_select_released);

    const error_token_observation_t cmd18_error =
        observe_cmd18_error_token(0x01U);
    const error_token_observation_t cmd18_zero =
        observe_cmd18_error_token(0x00U);
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR, cmd18_zero.result);
    CHECK(cmd18_zero.transfer_count > cmd18_error.transfer_count);
    CHECK_EQ(1U, cmd18_zero.cmd12_count);
    CHECK(cmd18_zero.chip_select_released);

    const error_token_observation_t cmd9_error =
        observe_cmd9_error_token(0x01U);
    const error_token_observation_t cmd9_zero =
        observe_cmd9_error_token(0x00U);
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR, cmd9_zero.result);
    CHECK(cmd9_zero.transfer_count > cmd9_error.transfer_count);
    CHECK(!cmd9_zero.initialized);
    CHECK(!cmd9_zero.spi_initialized);
    CHECK(cmd9_zero.all_gpio_deinitialized);
}

static void test_mock_command_configuration_validation(void)
{
    static uint8_t oversized_payload[PICO_MOCK_MAX_COMMAND_PAYLOAD + 1U];

    pico_mock_reset();
    CHECK(!pico_mock_sd_set_command(UINT8_MAX, 0x00U, NULL, 0U));
    CHECK(!pico_mock_sd_set_command(17U, 0x00U, NULL, 1U));
    CHECK(!pico_mock_sd_set_command(18U, 0x00U, oversized_payload,
        sizeof(oversized_payload)));
    CHECK(pico_mock_sd_set_command(18U, 0x00U, oversized_payload,
        PICO_MOCK_MAX_COMMAND_PAYLOAD));
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

static void test_reconfiguration_lifecycle(void)
{
    pico_mock_reset();
    sd_spi_t sd = { 0 };
    sd_spi_config_t config = valid_config();
    block_device_t *const device = initialize_sdhc(&sd);
    CHECK(device != NULL);

    config.baud_rate_hz = 8000000U;
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        sd_spi_configure(&sd, &config));
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        block_device_init(device));
    CHECK(sd.initialized);

    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, block_device_deinit(device));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    CHECK_EQ(8000000U, sd.config.baud_rate_hz);
}

static void test_sdhc_initialization(void)
{
    pico_mock_reset();
    sd_spi_t sd = { 0 };
    (void)initialize_sdhc(&sd);

    CHECK(sd.initialized);
    CHECK(!sd.card_type_legacy);
    CHECK(sd.card_type_hcxc);
    CHECK_EQ((uint64_t)SDHC_BLOCK_COUNT, sd.block_count);
    CHECK(pico_mock_spi_is_initialized());
    CHECK_EQ(400000U, pico_mock_spi_initial_baudrate());
    CHECK_EQ(OPERATIONAL_BAUDRATE, pico_mock_spi_baudrate());
    CHECK(pico_mock_gpio_was_initialized(PIN_CHIP_SELECT));
    CHECK(pico_mock_gpio_was_initialized(PIN_CARD_AVAILABLE));
    CHECK(pico_mock_gpio_was_pulled_up_at_init(PIN_CARD_AVAILABLE));
    CHECK_EQ(10U, pico_mock_gpio_read_count(PIN_CARD_AVAILABLE));
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
    CHECK_EQ(1U, pico_mock_sd_command_count(9U));
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
    configure_csd_v1();
    pico_mock_sd_set_command(16U, 0x00U, NULL, 0U);

    CHECK_EQ(BLOCK_DEVICE_RESULT_OK,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK(sd.initialized);
    CHECK(sd.card_type_legacy);
    CHECK(!sd.card_type_hcxc);
    CHECK_EQ((uint64_t)SDSC_BLOCK_COUNT, sd.block_count);
    CHECK_EQ(0U, pico_mock_sd_last_argument(41U));
    CHECK_EQ(1U, pico_mock_sd_command_count(16U));
    CHECK_EQ(512U, pico_mock_sd_last_argument(16U));
}

static void test_csd_capacity_encoding_limits(void)
{
    const sd_spi_config_t config = valid_config();
    sd_spi_t sd = { 0 };

    pico_mock_reset();
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    configure_successful_sdsc_card();
    configure_csd_v1_fields(0U, 0U, 9U);
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK_EQ(4ULL, sd.block_count);

    pico_mock_reset();
    memset(&sd, 0, sizeof(sd));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    configure_successful_sdsc_card();
    configure_csd_v1_fields(0x0fffU, 7U, 11U);
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK_EQ(8388608ULL, sd.block_count);

    pico_mock_reset();
    memset(&sd, 0, sizeof(sd));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    configure_successful_sdhc_card();
    configure_csd_v2_fields(0U);
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK_EQ(1024ULL, sd.block_count);

    pico_mock_reset();
    memset(&sd, 0, sizeof(sd));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    configure_successful_sdhc_card();
    configure_csd_v2_fields(0x3fffffU);
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK_EQ(4294967296ULL, sd.block_count);
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
    CHECK(!pico_mock_spi_is_initialized());
    CHECK(pico_mock_gpio_was_deinitialized(PIN_CARD_AVAILABLE));
    CHECK_EQ(30U, pico_mock_gpio_read_count(PIN_CARD_AVAILABLE));
}

static void test_card_detect_debounce(void)
{
    static const bool settles_present[] = {
        false, false, true, false, true,
        false, false, false, false, false,
        false, false, false, false, false,
    };
    static const bool never_stable[] = {
        false, false, false, false, false, false, false, false, false, true,
        false, false, false, false, false, false, false, false, false, true,
        false, false, false, false, false, false, false, false, false, true,
    };
    const sd_spi_config_t config = valid_config();
    sd_spi_t sd = { 0 };

    pico_mock_reset();
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    configure_successful_sdhc_card();
    CHECK(pico_mock_gpio_set_input_sequence(PIN_CARD_AVAILABLE,
        settles_present, sizeof(settles_present) / sizeof(settles_present[0])));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK(sd.initialized);
    CHECK_EQ(15U, pico_mock_gpio_read_count(PIN_CARD_AVAILABLE));
    CHECK(pico_mock_spi_is_initialized());

    pico_mock_reset();
    memset(&sd, 0, sizeof(sd));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    CHECK(pico_mock_gpio_set_input_sequence(PIN_CARD_AVAILABLE,
        never_stable, sizeof(never_stable) / sizeof(never_stable[0])));
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_DEVICE,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK(!sd.initialized);
    CHECK_EQ(30U, pico_mock_gpio_read_count(PIN_CARD_AVAILABLE));
    CHECK(!pico_mock_spi_is_initialized());
    CHECK_EQ(0U, pico_mock_sd_command_count(0U));
    CHECK(pico_mock_gpio_was_deinitialized(PIN_CARD_AVAILABLE));
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
    CHECK_EQ(0U, sd.block_count);
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
    CHECK_EQ(BLOCK_DEVICE_RESULT_BUSY_TIMEOUT, block_device_deinit(device));
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

static void test_block_device_wrapper_validation(void)
{
    uint8_t block[SD_BLOCK_SIZE] = { 0 };
    block_device_info_t info = { 0 };

    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT, block_device_init(NULL));
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT, block_device_deinit(NULL));
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        block_device_read_blocks(NULL, 0U, block, 1U));
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        block_device_write_blocks(NULL, 0U, block, 1U));
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        block_device_get_info(NULL, &info));

    const block_device_t empty = { 0 };
    CHECK(!block_device_is_valid(&empty));
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        block_device_init(&empty));
}

static void test_backend_context_validation(void)
{
    uint8_t block[SD_BLOCK_SIZE] = { 0 };
    block_device_info_t info = { 0 };
    sd_spi_t sd = { 0 };
    const sd_spi_config_t config = valid_config();

    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    const block_device_operations_t *const operations =
        sd.block_device.operations;
    sd.configured = false;

    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        operations->init(&sd));
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        operations->deinit(&sd));
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        operations->read_blocks(&sd, 0U, block, 1U));
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        operations->write_blocks(&sd, 0U, block, 1U));
    CHECK_EQ(BLOCK_DEVICE_RESULT_INVALID_ARGUMENT,
        operations->get_info(&sd, &info));
}

static void test_initialization_response_validation(void)
{
    sd_spi_config_t config = valid_config();
    sd_spi_t sd = { 0 };
    static const uint8_t bad_r7[] = { 0x00U, 0x00U, 0x00U, 0x00U };
    static const uint8_t good_r7[] = { 0x00U, 0x00U, 0x01U, 0xaaU };
    static const uint8_t unpowered_ocr[] = { 0x40U, 0x00U, 0x00U, 0x00U };
    static const uint8_t legacy_ocr[] = { 0x80U, 0x00U, 0x00U, 0x00U };

    pico_mock_reset();
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK(!sd.initialized);
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));
    CHECK(!pico_mock_spi_is_initialized());
    CHECK(pico_mock_gpio_was_deinitialized(PIN_CLOCK));
    CHECK(pico_mock_gpio_was_deinitialized(PIN_CHIP_SELECT));
    CHECK(pico_mock_gpio_was_deinitialized(PIN_CARD_AVAILABLE));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));

    pico_mock_reset();
    memset(&sd, 0, sizeof(sd));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    pico_mock_sd_set_command(0U, 0x01U, NULL, 0U);
    pico_mock_sd_set_command(8U, 0x01U, bad_r7, sizeof(bad_r7));
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK(!sd.initialized);
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));

    pico_mock_reset();
    memset(&sd, 0, sizeof(sd));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    pico_mock_sd_set_command(0U, 0x01U, NULL, 0U);
    pico_mock_sd_set_command(8U, 0x01U, good_r7, sizeof(good_r7));
    pico_mock_sd_set_command(55U, 0x01U, NULL, 0U);
    pico_mock_sd_set_command(41U, 0x00U, NULL, 0U);
    pico_mock_sd_set_command(58U, 0x00U,
        unpowered_ocr, sizeof(unpowered_ocr));
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK(!sd.initialized);

    pico_mock_reset();
    memset(&sd, 0, sizeof(sd));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    pico_mock_sd_set_command(0U, 0x01U, NULL, 0U);
    pico_mock_sd_set_command(8U, 0x05U, NULL, 0U);
    pico_mock_sd_set_command(55U, 0x01U, NULL, 0U);
    pico_mock_sd_set_command(41U, 0x00U, NULL, 0U);
    pico_mock_sd_set_command(58U, 0x00U, legacy_ocr, sizeof(legacy_ocr));
    configure_csd_v1();
    pico_mock_sd_set_command(16U, 0x04U, NULL, 0U);
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK(!sd.initialized);
}

static void test_unsupported_csd_rolls_back_initialization(void)
{
    uint8_t payload[19] = { 0 };
    payload[0] = 0xfeU;
    payload[1] = 0x80U;

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    const sd_spi_config_t config = valid_config();
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    configure_successful_sdhc_card();
    pico_mock_sd_set_command(9U, 0x00U, payload, sizeof(payload));

    CHECK_EQ(BLOCK_DEVICE_RESULT_NOT_IMPLEMENTED,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK(!sd.initialized);
    CHECK_EQ(0U, sd.block_count);
    CHECK(!pico_mock_spi_is_initialized());
    CHECK(pico_mock_gpio_was_deinitialized(PIN_CLOCK));
    CHECK(pico_mock_gpio_was_deinitialized(PIN_CARD_AVAILABLE));
}

static void test_cmd9_response_and_data_token_failures(void)
{
    const sd_spi_config_t config = valid_config();
    static const uint8_t error_token[] = { 0x0bU };
    sd_spi_t sd = { 0 };

    pico_mock_reset();
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    configure_successful_sdhc_card();
    pico_mock_sd_set_command(9U, 0x04U, NULL, 0U);
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK_EQ(0U, sd.block_count);
    CHECK(!pico_mock_spi_is_initialized());

    pico_mock_reset();
    memset(&sd, 0, sizeof(sd));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    configure_successful_sdhc_card();
    pico_mock_sd_set_command(9U, 0x00U, NULL, 0U);
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK_EQ(0U, sd.block_count);
    CHECK_EQ(1U, pico_mock_sd_command_count(9U));
    CHECK(pico_mock_spi_transfer_count() < 512U);

    pico_mock_reset();
    memset(&sd, 0, sizeof(sd));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    configure_successful_sdhc_card();
    pico_mock_sd_set_command(9U, 0x00U,
        error_token, sizeof(error_token));
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK_EQ(0U, sd.block_count);
    CHECK(!pico_mock_spi_is_initialized());
}

static void test_capacity_state_cleared_across_reinitialization(void)
{
    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *const device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    CHECK_EQ((uint64_t)SDHC_BLOCK_COUNT, sd.block_count);

    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, block_device_deinit(device));
    CHECK_EQ(0U, sd.block_count);

    configure_successful_sdhc_card();
    pico_mock_sd_set_command(9U, 0x04U, NULL, 0U);
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR, block_device_init(device));
    CHECK(!sd.initialized);
    CHECK_EQ(0U, sd.block_count);
}

static void test_single_block_read_sdhc(void)
{
    enum { PAYLOAD_SIZE = SD_BLOCK_SIZE + 3 };
    uint8_t payload[PAYLOAD_SIZE];
    uint8_t storage[SD_BLOCK_SIZE + 2U];
    const uint8_t seed = 0x21U;

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *const device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    CHECK_EQ(PAYLOAD_SIZE, make_read_payload(payload, 1U, seed));
    pico_mock_sd_set_command(17U, 0x00U, payload, sizeof(payload));
    memset(storage, 0xa5, sizeof(storage));

    CHECK_EQ(BLOCK_DEVICE_RESULT_OK,
        block_device_read_blocks(device, 123U, &storage[1], 1U));
    CHECK(buffer_matches(&storage[1], 1U, seed));
    CHECK_EQ(0xa5U, storage[0]);
    CHECK_EQ(0xa5U, storage[SD_BLOCK_SIZE + 1U]);
    CHECK_EQ(1U, pico_mock_sd_command_count(17U));
    CHECK_EQ(123U, pico_mock_sd_last_argument(17U));
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));
    CHECK_EQ(0U, pico_mock_sd_pending_response_count());
}

static void test_single_block_read_sdsc_uses_byte_address(void)
{
    enum { PAYLOAD_SIZE = SD_BLOCK_SIZE + 3 };
    uint8_t payload[PAYLOAD_SIZE];
    uint8_t block[SD_BLOCK_SIZE];

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *const device = initialize_legacy(&sd);
    CHECK(device != NULL);
    (void)make_read_payload(payload, 1U, 0x42U);
    pico_mock_sd_set_command(17U, 0x00U, payload, sizeof(payload));

    CHECK_EQ(BLOCK_DEVICE_RESULT_OK,
        block_device_read_blocks(device, 7U, block, 1U));
    CHECK(buffer_matches(block, 1U, 0x42U));
    CHECK_EQ(7U * SD_BLOCK_SIZE, pico_mock_sd_last_argument(17U));
}

static void test_single_block_read_failures(void)
{
    uint8_t block[SD_BLOCK_SIZE];
    static const uint8_t error_token[] = { 0x0bU };

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    pico_mock_sd_set_command(17U, 0x04U, NULL, 0U);
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR,
        block_device_read_blocks(device, 0U, block, 1U));
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));

    pico_mock_reset();
    memset(&sd, 0, sizeof(sd));
    device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    pico_mock_sd_set_command(17U, 0x00U,
        error_token, sizeof(error_token));
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR,
        block_device_read_blocks(device, 0U, block, 1U));
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));

    pico_mock_reset();
    memset(&sd, 0, sizeof(sd));
    device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    pico_mock_sd_set_command(17U, 0x00U, NULL, 0U);
    const size_t transfers_before = pico_mock_spi_transfer_count();
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR,
        block_device_read_blocks(device, 0U, block, 1U));
    CHECK(pico_mock_spi_transfer_count() - transfers_before < 256U);
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));
}

static void test_multiple_block_read(void)
{
    enum {
        BLOCK_COUNT = 4,
        PAYLOAD_SIZE = BLOCK_COUNT * (SD_BLOCK_SIZE + 3),
    };
    uint8_t payload[PAYLOAD_SIZE];
    uint8_t storage[(BLOCK_COUNT * SD_BLOCK_SIZE) + 2U];
    const uint8_t seed = 0x33U;

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *const device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    CHECK_EQ(PAYLOAD_SIZE,
        make_read_payload(payload, BLOCK_COUNT, seed));
    CHECK(pico_mock_sd_set_command(
        18U, 0x00U, payload, sizeof(payload)));
    CHECK(pico_mock_sd_set_command(12U, 0x00U, NULL, 0U));
    memset(storage, 0x5a, sizeof(storage));
    const size_t transfers_before = pico_mock_spi_transfer_count();

    CHECK_EQ(BLOCK_DEVICE_RESULT_OK,
        block_device_read_blocks(device, 456U, &storage[1], BLOCK_COUNT));
    CHECK(buffer_matches(&storage[1], BLOCK_COUNT, seed));
    CHECK_EQ(0x5aU, storage[0]);
    CHECK_EQ(0x5aU, storage[(BLOCK_COUNT * SD_BLOCK_SIZE) + 1U]);
    CHECK_EQ(1U, pico_mock_sd_command_count(18U));
    CHECK_EQ(456U, pico_mock_sd_last_argument(18U));
    CHECK_EQ(1U, pico_mock_sd_command_count(12U));
    CHECK_EQ(0U, pico_mock_sd_last_argument(12U));
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));
    CHECK_EQ(0U, pico_mock_sd_pending_response_count());

    const size_t stop_offset = transfers_before + 8U
        + (BLOCK_COUNT * (SD_BLOCK_SIZE + 3U));
    const uint8_t *const tx_log = pico_mock_spi_tx_log();
    CHECK_EQ(12U | 0x40U, tx_log[stop_offset]);
    CHECK_EQ(0U, tx_log[stop_offset + 1U]);
    CHECK_EQ(0U, tx_log[stop_offset + 2U]);
    CHECK_EQ(0U, tx_log[stop_offset + 3U]);
    CHECK_EQ(0U, tx_log[stop_offset + 4U]);
    CHECK_EQ(1U, tx_log[stop_offset + 5U]);
    CHECK_EQ(2078U,
        pico_mock_spi_transfer_count() - transfers_before);
}

static void test_multiple_block_timeout_is_not_success(void)
{
    enum {
        WAIT_BYTES = 101,
        PAYLOAD_SIZE = WAIT_BYTES + SD_BLOCK_SIZE + 3,
    };
    uint8_t payload[PAYLOAD_SIZE];
    uint8_t blocks[2U * SD_BLOCK_SIZE];

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *const device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    memset(payload, 0xff, WAIT_BYTES);
    CHECK_EQ(SD_BLOCK_SIZE + 3,
        make_read_payload(&payload[WAIT_BYTES], 1U, 0x55U));
    pico_mock_sd_set_command(18U, 0x00U, payload, sizeof(payload));
    pico_mock_sd_set_command(12U, 0x00U, NULL, 0U);

    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR,
        block_device_read_blocks(device, 0U, blocks, 2U));
    CHECK_EQ(1U, pico_mock_sd_command_count(12U));
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));
}

static void test_multiple_block_error_cleanup(void)
{
    uint8_t blocks[2U * SD_BLOCK_SIZE];
    static const uint8_t error_token[] = { 0x0dU };

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    pico_mock_sd_set_command(18U, 0x00U,
        error_token, sizeof(error_token));
    pico_mock_sd_set_command(12U, 0x00U, NULL, 0U);
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR,
        block_device_read_blocks(device, 0U, blocks, 2U));
    CHECK_EQ(1U, pico_mock_sd_command_count(12U));
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));

    pico_mock_reset();
    memset(&sd, 0, sizeof(sd));
    device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    pico_mock_sd_set_command(18U, 0x00U,
        error_token, sizeof(error_token));
    pico_mock_sd_set_command(12U, 0x04U, NULL, 0U);
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR,
        block_device_read_blocks(device, 0U, blocks, 2U));
    CHECK_EQ(1U, pico_mock_sd_command_count(12U));
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));

    pico_mock_reset();
    memset(&sd, 0, sizeof(sd));
    device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    pico_mock_sd_set_command(18U, 0x04U, NULL, 0U);
    pico_mock_sd_set_command(12U, 0x00U, NULL, 0U);
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR,
        block_device_read_blocks(device, 0U, blocks, 2U));
    CHECK_EQ(0U, pico_mock_sd_command_count(12U));
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));
}

static void test_multiple_block_error_recovery(void)
{
    enum { PAYLOAD_SIZE = SD_BLOCK_SIZE + 3 };
    uint8_t payload[PAYLOAD_SIZE];
    uint8_t failed_blocks[2U * SD_BLOCK_SIZE];
    uint8_t recovered_block[SD_BLOCK_SIZE];
    static const uint8_t error_token[] = { 0x08U };
    const uint8_t seed = 0x91U;

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *const device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    CHECK(pico_mock_sd_set_command(18U, 0x00U,
        error_token, sizeof(error_token)));
    CHECK(pico_mock_sd_set_command(12U, 0x00U, NULL, 0U));

    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR,
        block_device_read_blocks(device, 0U, failed_blocks, 2U));
    CHECK_EQ(1U, pico_mock_sd_command_count(18U));
    CHECK_EQ(1U, pico_mock_sd_command_count(12U));
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));

    CHECK_EQ(PAYLOAD_SIZE, make_read_payload(payload, 1U, seed));
    CHECK(pico_mock_sd_set_command(
        17U, 0x00U, payload, sizeof(payload)));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK,
        block_device_read_blocks(device, 2U, recovered_block, 1U));
    CHECK(buffer_matches(recovered_block, 1U, seed));
    CHECK_EQ(1U, pico_mock_sd_command_count(17U));
    CHECK_EQ(1U, pico_mock_sd_command_count(18U));
    CHECK_EQ(1U, pico_mock_sd_command_count(12U));
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));
    CHECK_EQ(0U, pico_mock_sd_pending_response_count());
}

static void test_multiple_block_stop_response_timeout_is_bounded(void)
{
    enum { PAYLOAD_SIZE = 2 * (SD_BLOCK_SIZE + 3) };
    uint8_t payload[PAYLOAD_SIZE];
    uint8_t blocks[2U * SD_BLOCK_SIZE];

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *const device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    (void)make_read_payload(payload, 2U, 0x67U);
    pico_mock_sd_set_command(18U, 0x00U, payload, sizeof(payload));
    const size_t transfers_before = pico_mock_spi_transfer_count();

    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR,
        block_device_read_blocks(device, 0U, blocks, 2U));
    CHECK_EQ(1U, pico_mock_sd_command_count(12U));
    CHECK(pico_mock_spi_transfer_count() - transfers_before < 1200U);
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));
}

static void test_multiple_block_stop_waits_until_ready(void)
{
    enum { PAYLOAD_SIZE = 2 * (SD_BLOCK_SIZE + 3) };
    uint8_t payload[PAYLOAD_SIZE];
    uint8_t blocks[2U * SD_BLOCK_SIZE];
    static const uint8_t stop_busy[] = { 0x00U, 0x00U, 0xffU };

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    (void)make_read_payload(payload, 2U, 0x61U);
    pico_mock_sd_set_command(18U, 0x00U, payload, sizeof(payload));
    pico_mock_sd_set_command(12U, 0x00U,
        stop_busy, sizeof(stop_busy));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK,
        block_device_read_blocks(device, 0U, blocks, 2U));
    CHECK_EQ(0U, pico_mock_sd_pending_response_count());
    CHECK_EQ(1U, pico_mock_sd_command_count(12U));
}

static void test_multiple_block_stop_failure_only_sent_once(void)
{
    enum { PAYLOAD_SIZE = 2 * (SD_BLOCK_SIZE + 3) };
    uint8_t payload[PAYLOAD_SIZE];
    uint8_t blocks[2U * SD_BLOCK_SIZE];

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *const device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    (void)make_read_payload(payload, 2U, 0x61U);
    pico_mock_sd_set_command(18U, 0x00U, payload, sizeof(payload));
    pico_mock_sd_set_command(12U, 0x04U, NULL, 0U);
    CHECK_EQ(BLOCK_DEVICE_RESULT_IO_ERROR,
        block_device_read_blocks(device, 0U, blocks, 2U));
    CHECK_EQ(1U, pico_mock_sd_command_count(12U));
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));
}

static void test_read_range_uses_csd_capacity(void)
{
    enum { PAYLOAD_SIZE = SD_BLOCK_SIZE + 3 };
    uint8_t payload[PAYLOAD_SIZE];
    uint8_t block[SD_BLOCK_SIZE];
    (void)make_read_payload(payload, 1U, 0x72U);

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    pico_mock_sd_set_command(17U, 0x00U, payload, sizeof(payload));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK,
        block_device_read_blocks(device, sd.block_count - 1U, block, 1U));
    CHECK_EQ((uint32_t)(sd.block_count - 1U),
        pico_mock_sd_last_argument(17U));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OUT_OF_RANGE,
        block_device_read_blocks(device, sd.block_count - 1U, block, 2U));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OUT_OF_RANGE,
        block_device_read_blocks(device, sd.block_count, block, 1U));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OUT_OF_RANGE,
        block_device_read_blocks(device, (uint64_t)UINT32_MAX + 1U,
            block, 1U));
    CHECK_EQ(1U, pico_mock_sd_command_count(17U));
    CHECK_EQ(0U, pico_mock_sd_command_count(18U));

    pico_mock_reset();
    memset(&sd, 0, sizeof(sd));
    device = initialize_legacy(&sd);
    CHECK(device != NULL);
    pico_mock_sd_set_command(17U, 0x00U, payload, sizeof(payload));
    CHECK_EQ(BLOCK_DEVICE_RESULT_OUT_OF_RANGE,
        block_device_read_blocks(device,
            ((uint64_t)UINT32_MAX / SD_BLOCK_SIZE) + 1U, block, 1U));
    CHECK_EQ(0U, pico_mock_sd_command_count(17U));
}

static void test_unimplemented_operations_contract(void)
{
    uint8_t block[SD_BLOCK_SIZE] = { 0 };
    block_device_info_t info = {
        .block_size_bytes = 0xaaaaU,
        .block_count = 0xbbbbU,
        .writable = true,
    };

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *const device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    const size_t transfers_before = pico_mock_spi_transfer_count();

    CHECK_EQ(BLOCK_DEVICE_RESULT_NOT_IMPLEMENTED,
        block_device_write_blocks(device, 9U, block, 1U));
    CHECK_EQ(BLOCK_DEVICE_RESULT_NOT_IMPLEMENTED,
        block_device_get_info(device, &info));
    CHECK_EQ(transfers_before, pico_mock_spi_transfer_count());
    CHECK_EQ(0xaaaaU, info.block_size_bytes);
    CHECK_EQ(0xbbbbU, info.block_count);
    CHECK(info.writable);
}

static void test_initialization_reports_busy_timeout(void)
{
    pico_mock_reset();
    sd_spi_t sd = { 0 };
    const sd_spi_config_t config = valid_config();
    CHECK_EQ(BLOCK_DEVICE_RESULT_OK, sd_spi_configure(&sd, &config));
    pico_mock_sd_set_busy_cycles(2000U);

    CHECK_EQ(BLOCK_DEVICE_RESULT_BUSY_TIMEOUT,
        block_device_init(sd_spi_as_block_device(&sd)));
    CHECK(!sd.initialized);
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));
}

static void test_read_command_reports_busy_timeout(void)
{
    uint8_t block[SD_BLOCK_SIZE] = { 0 };

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *const device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    pico_mock_sd_set_busy_cycles(2000U);

    CHECK_EQ(BLOCK_DEVICE_RESULT_BUSY_TIMEOUT,
        block_device_read_blocks(device, 0U, block, 1U));
    CHECK_EQ(0U, pico_mock_sd_command_count(17U));
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));
}

static void test_stop_transmission_reports_busy_timeout(void)
{
    enum { PAYLOAD_SIZE = 2 * (SD_BLOCK_SIZE + 3) };
    uint8_t payload[PAYLOAD_SIZE];
    uint8_t blocks[2U * SD_BLOCK_SIZE];
    static uint8_t busy_response[1200];

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *const device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    (void)make_read_payload(payload, 2U, 0x81U);
    memset(busy_response, 0, sizeof(busy_response));
    pico_mock_sd_set_command(18U, 0x00U, payload, sizeof(payload));
    pico_mock_sd_set_command(12U, 0x00U,
        busy_response, sizeof(busy_response));

    CHECK_EQ(BLOCK_DEVICE_RESULT_BUSY_TIMEOUT,
        block_device_read_blocks(device, 0U, blocks, 2U));
    CHECK_EQ(1U, pico_mock_sd_command_count(12U));
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));
}

static void test_error_cleanup_reports_busy_timeout(void)
{
    uint8_t blocks[2U * SD_BLOCK_SIZE];
    static const uint8_t error_token[] = { 0x08U };
    static uint8_t busy_response[1200];

    pico_mock_reset();
    sd_spi_t sd = { 0 };
    block_device_t *const device = initialize_sdhc(&sd);
    CHECK(device != NULL);
    memset(busy_response, 0, sizeof(busy_response));
    CHECK(pico_mock_sd_set_command(18U, 0x00U,
        error_token, sizeof(error_token)));
    CHECK(pico_mock_sd_set_command(12U, 0x00U,
        busy_response, sizeof(busy_response)));

    CHECK_EQ(BLOCK_DEVICE_RESULT_BUSY_TIMEOUT,
        block_device_read_blocks(device, 0U, blocks, 2U));
    CHECK_EQ(1U, pico_mock_sd_command_count(12U));
    CHECK(pico_mock_gpio_level(PIN_CHIP_SELECT));
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
    run_test(test_block_device_wrapper_validation,
        "block device wrapper validation");
    run_test(test_backend_context_validation,
        "backend context validation");
    run_test(test_configure_validation, "configure validation");
    run_test(test_mock_command_configuration_validation,
        "mock command configuration validation");
    run_test(test_configure_exposes_uninitialized_device,
        "configured device preconditions");
    run_test(test_reconfiguration_lifecycle,
        "reconfiguration lifecycle");
    run_test(test_sdhc_initialization, "SDHC initialization");
    run_test(test_legacy_card_initialization, "legacy card initialization");
    run_test(test_csd_capacity_encoding_limits,
        "CSD capacity encoding limits");
    run_test(test_missing_card_fails_initialization,
        "missing card initialization failure");
    run_test(test_card_detect_debounce, "card-detect debounce");
    run_test(test_card_initialization_timeout_is_bounded,
        "card initialization timeout");
    run_test(test_initialization_response_validation,
        "initialization response validation");
    run_test(test_unsupported_csd_rolls_back_initialization,
        "unsupported CSD initialization rollback");
    run_test(test_cmd9_response_and_data_token_failures,
        "CMD9 response and data-token failures");
    run_test(test_data_error_token_01_is_immediate,
        "data-error token 0x01 is immediate");
    run_test(test_data_error_token_02_is_immediate,
        "data-error token 0x02 is immediate");
    run_test(test_data_error_token_04_is_immediate,
        "data-error token 0x04 is immediate");
    run_test(test_data_error_token_08_is_immediate,
        "data-error token 0x08 is immediate");
    run_test(test_zero_is_not_a_data_error_token,
        "zero is not a data-error token");
    run_test(test_capacity_state_cleared_across_reinitialization,
        "capacity state reinitialization lifecycle");
    run_test(test_deinit_waits_and_releases_resources,
        "deinit waits and releases resources");
    run_test(test_deinit_timeout_preserves_resources,
        "deinit timeout preserves resources");
    run_test(test_deinit_before_init_does_not_transfer,
        "deinit before init");
    run_test(test_single_block_read_sdhc, "single-block SDHC read");
    run_test(test_single_block_read_sdsc_uses_byte_address,
        "single-block SDSC byte address");
    run_test(test_single_block_read_failures,
        "single-block read failures");
    run_test(test_multiple_block_read, "multiple-block read");
    run_test(test_multiple_block_timeout_is_not_success,
        "multiple-block timeout");
    run_test(test_multiple_block_error_cleanup,
        "multiple-block error cleanup");
    run_test(test_multiple_block_error_recovery,
        "multiple-block error recovery");
    run_test(test_multiple_block_stop_response_timeout_is_bounded,
        "multiple-block stop response timeout");
    run_test(test_multiple_block_stop_waits_until_ready,
        "multiple-block stop waits until ready");
    run_test(test_multiple_block_stop_failure_only_sent_once,
        "multiple-block stop failure sent once");
    run_test(test_read_range_uses_csd_capacity,
        "CSD capacity read range");
    run_test(test_unimplemented_operations_contract,
        "unimplemented operation contracts");
    run_test(test_initialization_reports_busy_timeout,
        "initialization busy timeout");
    run_test(test_read_command_reports_busy_timeout,
        "read command busy timeout");
    run_test(test_stop_transmission_reports_busy_timeout,
        "stop transmission busy timeout");
    run_test(test_error_cleanup_reports_busy_timeout,
        "error cleanup busy timeout");

    if (failures != 0) {
        (void)fprintf(stderr, "%d of %d tests failed\n", failures, tests_run);
        return 1;
    }

    (void)printf("All %d tests passed\n", tests_run);
    return 0;
}
