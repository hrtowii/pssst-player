#ifndef MT19937_H
#define MT19937_H

#include <stdint.h>

void     mt_seed(uint32_t seed);
uint32_t mt_next(void);              // returns full 32-bit random value
uint32_t mt_range(uint32_t bound);   // returns value in [0, bound)

#endif
