/* Test-only entry-point rename; execute the real foreground loop. */
int firmware_main(void);
#define main firmware_main
#include "../../src/main.c"
