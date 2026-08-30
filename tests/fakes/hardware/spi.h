#ifndef TAVERNKEEP_TEST_FAKE_HARDWARE_SPI_H
#define TAVERNKEEP_TEST_FAKE_HARDWARE_SPI_H

#include <stddef.h>
#include <stdint.h>

typedef struct spi_inst {
    unsigned int id;
} spi_inst_t;

unsigned int spi_init(spi_inst_t *spi, unsigned int baudrate);
void spi_deinit(spi_inst_t *spi);
unsigned int spi_set_baudrate(spi_inst_t *spi, unsigned int baudrate);
int spi_write_read_blocking(
    spi_inst_t *spi,
    const uint8_t *source,
    uint8_t *destination,
    size_t length);

#endif

