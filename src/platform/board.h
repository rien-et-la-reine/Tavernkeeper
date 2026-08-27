#ifndef TAVERNKEEP_PLATFORM_BOARD_H
#define TAVERNKEEP_PLATFORM_BOARD_H

#include <stdbool.h>

void board_init(void);

bool board_status_led_available(void);
bool board_status_led_set(bool on);

#endif

