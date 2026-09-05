#ifndef TAVERNKEEP_TEST_FAKE_HARDWARE_IRQ_H
#define TAVERNKEEP_TEST_FAKE_HARDWARE_IRQ_H

#include <stdbool.h>

enum {
    IO_IRQ_BANK0 = 0,
};

void irq_set_enabled(unsigned int interrupt, bool enabled);

#endif
