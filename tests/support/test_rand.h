/*
 * Deterministic PRNG for property and fuzz tests.
 *
 * Randomised tests are only useful if a failure can be reproduced exactly, so
 * every generator here is seeded explicitly and every suite that uses one
 * prints the seed it ran with and accepts --seed to replay it.
 */
#ifndef TAVERNKEEP_TEST_RAND_H
#define TAVERNKEEP_TEST_RAND_H

#include <stdint.h>

typedef struct { uint64_t state; } t_rand_t;

static inline void t_rand_seed(t_rand_t *rng, uint64_t seed)
{
    rng->state = seed ^ UINT64_C(0x9E3779B97F4A7C15);
    if (rng->state == 0U) {
        rng->state = UINT64_C(0x123456789ABCDEF);
    }
}

/* splitmix64: small, well-distributed, and identical on every host. */
static inline uint64_t t_rand_next(t_rand_t *rng)
{
    uint64_t z = (rng->state += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30U)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27U)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31U);
}

static inline uint64_t t_rand_below(t_rand_t *rng, uint64_t bound)
{
    return bound == 0U ? 0U : t_rand_next(rng) % bound;
}

static inline uint64_t t_rand_range(t_rand_t *rng, uint64_t low, uint64_t high)
{
    return low + t_rand_below(rng, (high - low) + 1U);
}

#endif
