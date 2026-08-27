#include "storage/sd_spi.h"

#include <stddef.h>

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
            || config->baud_rate_hz == 0U) {
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

    /*
     * TODO(owner): Configure GPIO/SPI at the SD-card initialization clock,
     * implement the card command/timeout sequence, then set initialized only
     * after the card is ready. Do not wait forever for a response.
     */
    return BLOCK_DEVICE_RESULT_NOT_IMPLEMENTED;
}

static block_device_result_t sd_spi_device_deinit(void *context)
{
    sd_spi_t *const sd = context;
    if (sd == NULL || !sd->configured) {
        return BLOCK_DEVICE_RESULT_INVALID_ARGUMENT;
    }

    /* TODO(owner): Release the SPI/GPIO resources owned by this backend. */
    return BLOCK_DEVICE_RESULT_NOT_IMPLEMENTED;
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

