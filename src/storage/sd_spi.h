#ifndef TAVERNKEEP_STORAGE_SD_SPI_H
#define TAVERNKEEP_STORAGE_SD_SPI_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/spi.h"

#include "storage/block_device.h"

typedef struct {
    spi_inst_t *spi;
    uint32_t baud_rate_hz;
    uint8_t pin_clock;
    uint8_t pin_controller_out;
    uint8_t pin_controller_in;
    uint8_t pin_chip_select;
} sd_spi_config_t;

/* Caller-owned state; no allocation is performed by this backend. */
typedef struct {
    block_device_t block_device;
    sd_spi_config_t config;
    bool configured;
    bool initialized;
} sd_spi_t;

block_device_result_t sd_spi_configure(
    sd_spi_t *sd,
    const sd_spi_config_t *config);

block_device_t *sd_spi_as_block_device(sd_spi_t *sd);

#endif

