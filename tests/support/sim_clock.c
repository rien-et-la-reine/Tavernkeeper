#include "sim_clock.h"

enum {
    SIM_CLOCK_DEFAULT_BAUD_HZ = 400000U,
    SIM_CLOCK_BITS_PER_BYTE = 8U,
};

static sim_clock_mode_t clock_mode;
static uint32_t baud_hz = SIM_CLOCK_DEFAULT_BAUD_HZ;
static uint64_t now_ns;
static uint64_t poll_count;
/* Sub-microsecond remainder, so fast baud rates do not round every byte to
 * zero and make a bounded loop look instantaneous. */

void sim_clock_reset(void)
{
    clock_mode = SIM_CLOCK_BUS_TIME;
    baud_hz = SIM_CLOCK_DEFAULT_BAUD_HZ;
    now_ns = 0U;
    poll_count = 0U;
}

void sim_clock_set_mode(sim_clock_mode_t mode)
{
    clock_mode = mode;
}

sim_clock_mode_t sim_clock_mode(void)
{
    return clock_mode;
}

void sim_clock_set_baud(uint32_t new_baud_hz)
{
    if (new_baud_hz != 0U) {
        baud_hz = new_baud_hz;
    }
}

uint32_t sim_clock_baud(void)
{
    return baud_hz;
}

uint64_t sim_clock_spi_byte_ns(void)
{
    /* Round up so a byte is never free; a loop that clocks bytes must make
     * progress against any timeout. */
    const uint64_t numerator =
        (uint64_t)SIM_CLOCK_BITS_PER_BYTE * UINT64_C(1000000000);
    return (numerator + baud_hz - 1U) / baud_hz;
}

uint64_t sim_clock_now_us(void)
{
    return now_ns / UINT64_C(1000);
}

void sim_clock_advance_us(uint64_t microseconds)
{
    now_ns += microseconds * UINT64_C(1000);
}

void sim_clock_charge_spi_byte(void)
{
    if (clock_mode == SIM_CLOCK_BUS_TIME) {
        now_ns += sim_clock_spi_byte_ns();
    }
}

uint64_t sim_clock_timeout_us(uint32_t milliseconds)
{
    return sim_clock_now_us() + (uint64_t)milliseconds * UINT64_C(1000);
}

bool sim_clock_time_reached(uint64_t target_us)
{
    poll_count++;
    const bool reached = sim_clock_now_us() >= target_us;
    if (clock_mode == SIM_CLOCK_POLL_TICK) {
        now_ns += UINT64_C(1000000); /* legacy: one millisecond per poll */
    }
    return reached;
}

void sim_clock_sleep_ms(uint32_t milliseconds)
{
    now_ns += (uint64_t)milliseconds * UINT64_C(1000000);
}

uint64_t sim_clock_poll_count(void)
{
    return poll_count;
}
