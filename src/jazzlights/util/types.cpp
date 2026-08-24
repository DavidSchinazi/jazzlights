#include "jazzlights/util/types.h"

#include "jazzlights/util/pseudorandom.h"

namespace jazzlights {

PatternBits RandomizePatternBits(PatternBits pattern) {
  bool reservedWithPalette = false;
  if ((pattern & 0xF) == 0) {  // Pattern is reserved.
    if ((pattern & 0xFF) == 0x30) {
      reservedWithPalette = true;
    } else {
      // Don't randomize reserved patterns unless they're reserved with palette.
      return pattern;
    }
  }
  pattern &= 0xC000E000;  // Clear bits 13-15 for palette and 30-31 for pattern.
  if (reservedWithPalette) {
    pattern |= 0x30;
  } else {
    pattern |= UnpredictableRandom::GetNumberBetween(1, (1 << 4) - 1);  // Randomize bits 0-3, but must not be all-zero.
    pattern |= UnpredictableRandom::GetNumberBetween(0, (1 << 9) - 1) << 4;  // Randomize bits 4-12.
  }
  pattern |= UnpredictableRandom::GetNumberBetween(0, (1 << 14) - 1) << 16;  // Randomize bits 16-29.
  return pattern;
}

}  // namespace jazzlights
