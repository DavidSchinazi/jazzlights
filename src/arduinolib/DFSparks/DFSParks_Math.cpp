#include "DFSparks_Math.h"

namespace dfsparks {

// X(n+1) = (2053 * X(n)) + 13849)
#define FASTLED_RAND16_2053 ((uint16_t)(2053))
#define FASTLED_RAND16_13849 ((uint16_t)(13849))

/// random number seed
static uint16_t rand16seed; // = RAND16_SEED;

/// Generate an 8-bit random number
uint8_t random8() {
  rand16seed = (rand16seed * FASTLED_RAND16_2053) + FASTLED_RAND16_13849;
  // return the sum of the high and low bytes, for better
  //  mixing and non-sequential correlation
  return (uint8_t)(((uint8_t)(rand16seed & 0xFF)) +
                   ((uint8_t)(rand16seed >> 8)));
}

/// Generate an 8-bit random number between 0 and lim
/// @param lim the upper bound for the result
uint8_t random8(uint8_t lim) {
  uint8_t r = random8();
  r = (r * lim) >> 8;
  return r;
}

/// Generate an 8-bit random number in the given range
/// @param min the lower bound for the random number
/// @param lim the upper bound for the random number
uint8_t random8(uint8_t min, uint8_t lim) {
  uint8_t delta = lim - min;
  uint8_t r = random8(delta) + min;
  return r;
}
} // namespace dfsparks
