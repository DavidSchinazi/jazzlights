#include "jazzlights/util/math.hpp"

namespace jazzlights {
namespace internal {

///         square root for 16-bit integers
///         About three times faster and five times smaller
///         than Arduino's general sqrt on AVR.
uint8_t sqrt16(uint16_t x) {
  if (x <= 1) {
    return x;
  }

  uint8_t low = 1; // lower bound
  uint8_t hi, mid;

  if (x > 7904) {
    hi = 255;
  } else {
    hi = (x >> 5) + 8; // initial estimate for upper bound
  }

  do {
    mid = (low + hi) >> 1;
    if ((uint16_t)(mid * mid) > x) {
      hi = mid - 1;
    } else {
      if (mid == 255) {
        return 255;
      }
      low = mid + 1;
    }
  } while (hi >= low);

  return low - 1;
}

} // namespace internal
}  // namespace jazzlights
