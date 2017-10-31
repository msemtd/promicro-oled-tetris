
 
#include "deb8.h"

uint8_t debouncer8(uint8_t sample, debounce8_t* p)
{
    if(!p)
        return 0;
    uint8_t delta;
    delta = sample ^ p->state;
    p->c1 = (p->c1 ^ p->c0) & delta;
    p->c0 = ~(p->c0) & delta;

    p->toggle = delta & ~(p->c0 | p->c1);
    p->state ^= p->toggle;

    return p->state;
}

