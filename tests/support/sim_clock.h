/*
 * Simulated time for host tests.
 *
 * WHY THIS EXISTS
 * ---------------
 * The original harness advanced fake time inside time_reached(): every poll
 * cost one simulated millisecond. That made every "timeout" test a disguised
 * assertion about how many times the driver happened to call time_reached(),
 * not about elapsed time. A driver that polled twice per loop iteration would
 * have "timed out" twice as fast, and a driver that consumed a million SPI
 * bytes between two time checks would never have timed out at all.
 *
 * SIM_CLOCK_BUS_TIME (the default) instead advances time from modelled work:
 *   - every SPI byte costs 8 bit-times at the currently programmed baud rate,
 *   - sleep_ms() costs what it says,
 *   - tests may advance the clock explicitly.
 * time_reached() becomes a pure query with no side effects.
 *
 * SIM_CLOCK_POLL_TICK reproduces the legacy behaviour and exists only so the
 * whole SD suite can be run under both models and any test that silently
 * depends on the artifact is exposed. Do not write new tests against it.
 *
 * WHAT THIS STILL IS NOT
 * ----------------------
 * Byte time is an idealisation: it ignores inter-byte gaps, SPI FIFO depth,
 * DMA, interrupt latency, card-internal timing variation and clock-domain
 * effects. It gives tests a defensible *lower bound* on how long a bounded
 * loop takes, and lets a test say "the card answered exactly at the timeout
 * boundary". It does not measure throughput or prove real-time behaviour.
 */
#ifndef TAVERNKEEP_TEST_SIM_CLOCK_H
#define TAVERNKEEP_TEST_SIM_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    /* Time advances with modelled bus work. Default; use this. */
    SIM_CLOCK_BUS_TIME = 0,
    /* Legacy: each time_reached() call costs one millisecond. */
    SIM_CLOCK_POLL_TICK,
} sim_clock_mode_t;

void sim_clock_reset(void);
void sim_clock_set_mode(sim_clock_mode_t mode);
sim_clock_mode_t sim_clock_mode(void);

/* Bit rate used to price SPI bytes. Set by the SPI fake on init/set_baudrate. */
void sim_clock_set_baud(uint32_t baud_hz);
uint32_t sim_clock_baud(void);

uint64_t sim_clock_now_us(void);
void sim_clock_advance_us(uint64_t microseconds);

/* Charge for one 8-bit SPI byte at the current baud rate. */
void sim_clock_charge_spi_byte(void);
/* Nanoseconds one byte costs, for tests that want to compute boundaries. */
uint64_t sim_clock_spi_byte_ns(void);

/* Called by the pico/time.h fake. */
uint64_t sim_clock_timeout_us(uint32_t milliseconds);
bool sim_clock_time_reached(uint64_t target_us);
void sim_clock_sleep_ms(uint32_t milliseconds);

/* How many times time_reached() has been asked. Lets a test separate
 * "the loop polled a lot" from "a lot of time passed". */
uint64_t sim_clock_poll_count(void);

#endif
