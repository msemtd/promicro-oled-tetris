
#ifndef __DEB__
#define __DEB__

#include <stdint.h>

typedef struct {
    uint8_t state;
    uint8_t c0;
    uint8_t c1;
    uint8_t toggle;
} debounce8_t;

uint8_t debouncer8(uint8_t sample, debounce8_t* p);

#endif

