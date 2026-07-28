#include "util/mt19937.h"
// rng for playlist shuffles
#define MT_N 624
#define MT_M 397
#define MT_MATRIX_A     0x9908b0dfUL
#define MT_UPPER_MASK   0x80000000UL // most significant bit
#define MT_LOWER_MASK   0x7fffffffUL // least significant 31 bits

static uint32_t mt_state[MT_N];
static int      mt_index = MT_N + 1; // signals "not seeded yet"

void mt_seed(uint32_t seed) {
    mt_state[0] = seed;
    for (int i = 1; i < MT_N; i++) {
        mt_state[i] = (1812433253UL * (mt_state[i - 1] ^ (mt_state[i - 1] >> 30)) + i);
    }
    mt_index = MT_N;
}

static void mt_generate(void) {
    static const uint32_t mag01[2] = { 0x0UL, MT_MATRIX_A };
    uint32_t y;
    int kk;

    if (mt_index == MT_N + 1) mt_seed(5489UL);

    for (kk = 0; kk < MT_N - MT_M; kk++) {
        y = (mt_state[kk] & MT_UPPER_MASK) | (mt_state[kk + 1] & MT_LOWER_MASK);
        mt_state[kk] = mt_state[kk + MT_M] ^ (y >> 1) ^ mag01[y & 0x1UL];
    }
    for (; kk < MT_N - 1; kk++) {
        y = (mt_state[kk] & MT_UPPER_MASK) | (mt_state[kk + 1] & MT_LOWER_MASK);
        mt_state[kk] = mt_state[kk + (MT_M - MT_N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
    }
    y = (mt_state[MT_N - 1] & MT_UPPER_MASK) | (mt_state[0] & MT_LOWER_MASK);
    mt_state[MT_N - 1] = mt_state[MT_M - 1] ^ (y >> 1) ^ mag01[y & 0x1UL];

    mt_index = 0;
}

uint32_t mt_next(void) {
    uint32_t y;

    if (mt_index >= MT_N) mt_generate();

    y = mt_state[mt_index++];

    // tempering
    y ^= (y >> 11);
    y ^= (y << 7)  & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);

    return y;
}

uint32_t mt_range(uint32_t bound) {
    if (bound == 0) return 0;
    // avoid modulo bias by rejecting values in the tail
    uint32_t limit = (0xFFFFFFFFUL / bound) * bound;
    uint32_t r;
    do { r = mt_next(); } while (r >= limit);
    return r % bound;
}
