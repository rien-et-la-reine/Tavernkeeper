#include "storage/sd_spi.h"

#include <stddef.h>

#include "hardware/gpio.h"
#include "pico/time.h"

static bool sd_spi_wait_ready(const sd_spi_t *sd);
static uint8_t sd_spi_transfer(const sd_spi_t *sd, uint8_t tx);
static uint8_t sd_spi_command(const sd_spi_t *sd, uint8_t cmd, uint32_t arg);
static void sd_spi_release_bus(const sd_spi_t *sd) {
    gpio_put(sd->config.pin_chip_select, true);
    sd_spi_transfer(sd, 0xFF);
}
static block_device_result_t sd_spi_device_init(void *context);
static block_device_result_t sd_spi_device_deinit(void *context);
static block_device_result_t sd_spi_device_read_blocks(
    void *context,
    uint64_t first_lba,
    void *buffer,
    size_t block_count);
static block_device_result_t sd_spi_device_write_blocks(
    void *context,
    uint64_t first_lba,
    const void *buffer,
    size_t block_count);
static block_device_result_t sd_spi_device_get_info(
    void *context,
    block_device_info_t *info);

static const block_device_operations_t sd_spi_operations = {
    .init = sd_spi_device_init,
    .deinit = sd_spi_device_deinit,
    .read_blocks = sd_spi_device_read_blocks,
    .write_blocks = sd_spi_device_write_blocks,
    .get_info = sd_spi_device_get_info,
};

block_device_result_t sd_spi_configure(
    sd_spi_t *sd,
    const sd_spi_config_t *config)
{
    if (sd == NULL || config == NULL || config->spi == NULL
            || config->baud_rate_hz == 0U || config->baud_rate_hz > 25000000U) {
        return BLOCK_DEVICE_RESULT_INVALID_ARGUMENT;
    }

    sd->config = *config;
    sd->configured = true;
    sd->initialized = false;
    sd->block_device.context = sd;
    sd->block_device.operations = &sd_spi_operations;

    return BLOCK_DEVICE_RESULT_OK;
}

block_device_t *sd_spi_as_block_device(sd_spi_t *sd)
{
    if (sd == NULL || !sd->configured) {
        return NULL;
    }
    return &sd->block_device;
}

static block_device_result_t sd_spi_device_init(void *context)
{
    sd_spi_t *const sd = context;
    if (sd == NULL || !sd->configured) {
        return BLOCK_DEVICE_RESULT_INVALID_ARGUMENT;
    }

    sd->initialized = false;
    sd->card_type_legacy = false;
    sd->card_type_hcxc = false;

    spi_init(sd->config.spi, 400000); //init using slower baud rate, will switch to higher one later
    gpio_init(sd->config.pin_chip_select);
    gpio_put(sd->config.pin_chip_select, true); //for safety to avoid accidentally asserting cs
    gpio_set_dir(sd->config.pin_chip_select, GPIO_OUT);
    gpio_init(sd->config.pin_card_available); // external hardware pullup required
    gpio_set_dir(sd->config.pin_card_available, GPIO_IN);
    gpio_set_function(sd->config.pin_clock, GPIO_FUNC_SPI);
    gpio_set_function(sd->config.pin_controller_in, GPIO_FUNC_SPI);
    gpio_set_function(sd->config.pin_controller_out, GPIO_FUNC_SPI);

    //check if card present and not write protected
    if (gpio_get(sd->config.pin_card_available) != 0) {
        return BLOCK_DEVICE_RESULT_INVALID_DEVICE;
    }

    //cs high
    gpio_put(sd->config.pin_chip_select, true);
    sleep_ms(1);
    //mosi high, 80 clock cycles
    for (uint8_t i = 0; i < 10; i++)
    {
        sd_spi_transfer(sd, 0xFF);
    }
    //cs low
    gpio_put(sd->config.pin_chip_select, false);

    uint8_t r1 = 0xFF;
    //cmd0
    r1 = sd_spi_command(sd, 0, 0);
    //check R1 = 0x01 (in idle state)
    if (r1 != 0x01) {
        sd_spi_release_bus(sd);
        return BLOCK_DEVICE_RESULT_IO_ERROR;
    }
    //cmd8 with arg 0x1AA
    r1 = sd_spi_command(sd, 8, 0x1AA);
    if (r1 == 0x01) {
        //read R7, card type is v2+
        sd_spi_transfer(sd, 0xFF);
        sd_spi_transfer(sd, 0xFF);
        if ((sd_spi_transfer(sd, 0xFF) & 0x0F) != 0x01) {
            sd_spi_release_bus(sd);
            return BLOCK_DEVICE_RESULT_IO_ERROR; 
        }
        if (sd_spi_transfer(sd, 0xFF) != 0xAA) { 
            sd_spi_release_bus(sd);
            return BLOCK_DEVICE_RESULT_IO_ERROR; 
        }
    } else if (r1 == 0x05) {
        //card type is v1
        sd->card_type_legacy = true;
    } else {
        sd_spi_release_bus(sd);
        return BLOCK_DEVICE_RESULT_IO_ERROR;
    }
    //omitting ocr check since we're operating at 3v3 anyway and supporting that's basically mandated by the spec for all cards
    //repeat ACMD41 until card not idle, make sure to set the HCS bit if card is v2
    absolute_time_t timeout = make_timeout_time_ms(1200); //slightly higher to account for starting timer before first attempt
    do {
        r1 = sd_spi_command(sd, 55, 0);
        if (r1 > 0x01) { 
            sd_spi_release_bus(sd);
            return BLOCK_DEVICE_RESULT_IO_ERROR; 
        }
        if (sd->card_type_legacy) {
            r1 = sd_spi_command(sd, 41, 0);
        } else {
            r1 = sd_spi_command(sd, 41, 0x40000000);
        }
        if (r1 > 0x01) { 
            sd_spi_release_bus(sd);
            return BLOCK_DEVICE_RESULT_IO_ERROR; 
        }
        //repeat ACMD41 until R1 response no longer reads IDLE_STATE
    } while (!time_reached(timeout) && (r1 == 0x01));
    if (r1 != 0x00) { 
        sd_spi_release_bus(sd);
        return BLOCK_DEVICE_RESULT_IO_ERROR; 
    }
    //cmd58 to get ocr, check ccs to verify SDSC or SDHC card (byte addressing vs block addressing)
    r1 = sd_spi_command(sd, 58, 0);
    if (r1 != 0x00) { 
        sd_spi_release_bus(sd);
        return BLOCK_DEVICE_RESULT_IO_ERROR; 
    }
    uint8_t ocr_msb = sd_spi_transfer(sd, 0xFF); //not R1 format, first byte of OCR
    sd_spi_transfer(sd, 0xFF);
    sd_spi_transfer(sd, 0xFF);
    sd_spi_transfer(sd, 0xFF);
    if ((ocr_msb & 0x80) != 0x80) {
        //not powered up for some reason
        sd_spi_release_bus(sd);
        return BLOCK_DEVICE_RESULT_IO_ERROR;
    }
    if (((ocr_msb & 0x40) == 0x40) && (!sd->card_type_legacy)) {
        sd->card_type_hcxc = true;
    }

    if (!sd->card_type_hcxc) {
        //for sdsc card need to set block length to 512 with cmd16
        r1 = sd_spi_command(sd, 16, 512);
        if (r1 != 0x00) { 
            sd_spi_release_bus(sd);
            return BLOCK_DEVICE_RESULT_IO_ERROR; 
        }
    }

    //cs deassert and trailing clocks
    sd_spi_release_bus(sd);
    
    //move to operational baud rate
    spi_set_baudrate(sd->config.spi, sd->config.baud_rate_hz);
    
    //card initalized, update status accordingly
    sd->initialized = true;
    
    return BLOCK_DEVICE_RESULT_OK;
}

// if IO_ERROR is returned, spi resources remain configured and device remains initialized. if a forced shutdown path becomes neccessary that will be handled elsewhere
static block_device_result_t sd_spi_device_deinit(void *context)
{
    sd_spi_t *const sd = context;
    if (sd == NULL || !sd->configured) {
        return BLOCK_DEVICE_RESULT_INVALID_ARGUMENT;
    }

    if (sd->initialized) {
        gpio_put(sd->config.pin_chip_select, false);
        if (!sd_spi_wait_ready(sd)) {
            sd_spi_release_bus(sd);
            return BLOCK_DEVICE_RESULT_IO_ERROR;
        }
        sd_spi_release_bus(sd);
    } else {
        gpio_put(sd->config.pin_chip_select, true);
    }

    spi_deinit(sd->config.spi);
    gpio_deinit(sd->config.pin_clock);
    gpio_deinit(sd->config.pin_controller_out);
    gpio_deinit(sd->config.pin_controller_in);
    gpio_deinit(sd->config.pin_chip_select);
    gpio_deinit(sd->config.pin_card_available);

    sd->initialized = false;
    sd->card_type_legacy = false;
    sd->card_type_hcxc = false;

    return BLOCK_DEVICE_RESULT_OK;
}

static block_device_result_t sd_spi_device_read_blocks(
    void *context,
    uint64_t first_lba,
    void *buffer,
    size_t block_count)
{
    sd_spi_t *const sd = context;
    (void)first_lba;
    (void)buffer;
    (void)block_count;

    if (sd == NULL || !sd->configured) {
        return BLOCK_DEVICE_RESULT_INVALID_ARGUMENT;
    }
    if (!sd->initialized) {
        return BLOCK_DEVICE_RESULT_NOT_INITIALIZED;
    }

    //requested block count check to determine command

    //check hcxc flag for block address vs. byte address

    //issue command and collect r1

    //validate r1 response

    //for each block:
    //wait for either data start or data error token (100ms timeout)
    //read in block and discard 2 byte crc

    //if multiblock:
    //cmd12 stop transmission
    //stuff byte
    //r1 response
    //wait_ready

    /* TODO(owner): Implement bounded-time SD SPI block reads. */
    return BLOCK_DEVICE_RESULT_NOT_IMPLEMENTED;
}

static block_device_result_t sd_spi_device_write_blocks(
    void *context,
    uint64_t first_lba,
    const void *buffer,
    size_t block_count)
{
    sd_spi_t *const sd = context;
    (void)first_lba;
    (void)buffer;
    (void)block_count;

    if (sd == NULL || !sd->configured) {
        return BLOCK_DEVICE_RESULT_INVALID_ARGUMENT;
    }
    if (!sd->initialized) {
        return BLOCK_DEVICE_RESULT_NOT_INITIALIZED;
    }

    /* TODO(owner): Implement bounded-time SD SPI block writes. */
    return BLOCK_DEVICE_RESULT_NOT_IMPLEMENTED;
}

static block_device_result_t sd_spi_device_get_info(
    void *context,
    block_device_info_t *info)
{
    sd_spi_t *const sd = context;
    (void)info;

    if (sd == NULL || !sd->configured) {
        return BLOCK_DEVICE_RESULT_INVALID_ARGUMENT;
    }
    if (!sd->initialized) {
        return BLOCK_DEVICE_RESULT_NOT_INITIALIZED;
    }

    /* TODO(owner): Report capacity parsed from the card CSD register. */
    return BLOCK_DEVICE_RESULT_NOT_IMPLEMENTED;
}

static bool sd_spi_wait_ready(const sd_spi_t *sd) {
    absolute_time_t timeout = make_timeout_time_ms(1000);
    while (!time_reached(timeout)) {
        if (sd_spi_transfer(sd, 0xFF) == 0xFF) {
            return true;
        }
    }

    return false;
}

static uint8_t sd_spi_transfer(const sd_spi_t *sd, uint8_t tx) {
    uint8_t rx;
    spi_write_read_blocking(sd->config.spi, &tx, &rx, 1);
    return rx;
}

static uint8_t sd_spi_command(const sd_spi_t *sd, uint8_t cmd, uint32_t arg) {
    uint8_t i = 0;
    uint8_t r1; // R1 response byte

    //ensure card is ready, if timed out pass that on
    if (!sd_spi_wait_ready(sd)) { return 0xFF; }
    //send command index cmd
    sd_spi_transfer(sd, cmd | 0x40);
    //send argument arg, big endian
    sd_spi_transfer(sd, (uint8_t) (arg >> 24));
    sd_spi_transfer(sd, (uint8_t) (arg >> 16));
    sd_spi_transfer(sd, (uint8_t) (arg >> 8));
    sd_spi_transfer(sd, (uint8_t) (arg));
    //send CRC if needed
    if (cmd == 0) {
        sd_spi_transfer(sd, 0x94|0x01);
    } else if (cmd == 8) {
        sd_spi_transfer(sd, 0x86|0x01);
    } else {
        sd_spi_transfer(sd, 0x00|0x01);
    }
    //wait for R1 response
    while (((r1 = sd_spi_transfer(sd, 0xFF)) & 0x80) == 0x80) {
        i++;
        if (i > 7) { return 0xFF; }
    }
    //returns R1 when recieved or 0xFF if timeout occured
    return r1;
}
