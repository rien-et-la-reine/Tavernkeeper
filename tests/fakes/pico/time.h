#ifndef TAVERNKEEP_TEST_FAKE_PICO_TIME_H
#define TAVERNKEEP_TEST_FAKE_PICO_TIME_H

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t absolute_time_t;

absolute_time_t make_timeout_time_ms(uint32_t milliseconds);
bool time_reached(absolute_time_t target);
void sleep_ms(uint32_t milliseconds);

#endif

