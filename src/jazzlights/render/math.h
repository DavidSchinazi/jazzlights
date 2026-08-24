#ifndef JL_RENDER_MATH_H
#define JL_RENDER_MATH_H

#define _USE_MATH_DEFINES
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "jazzlights/render/fastled_wrapper.h"

namespace jazzlights {

// The jlbeat* functions are almost identical to the corresponding beat* functions in FastLED/src/lib8tion.h, except
// that they operate on elapsedTime (the time since the current pattern has started) instead of the absolute time.

constexpr uint16_t JlBeat88(uint16_t beatsPerMinute88, uint32_t elapsedTime) {
  return (elapsedTime * beatsPerMinute88 * 280) >> 16;
}

inline uint16_t JlBeat16(uint16_t beatsPerMinute, uint32_t elapsedTime) {
  if (beatsPerMinute < 256) beatsPerMinute <<= 8;
  return JlBeat88(beatsPerMinute, elapsedTime);
}

inline uint8_t JlBeat8(uint16_t beatsPerMinute, uint32_t elapsedTime) {
  return JlBeat16(beatsPerMinute, elapsedTime) >> 8;
}

inline uint8_t JlBeatSin8(uint16_t beatsPerMinute, uint32_t elapsedTime, uint8_t lowest = 0, uint8_t highest = 255,
                          uint8_t phaseOffset = 0) {
  uint8_t beat = JlBeat8(beatsPerMinute, elapsedTime);
  uint8_t beatSin = sin8(beat + phaseOffset);
  uint8_t rangeWidth = highest - lowest;
  uint8_t scaledBeat = scale8(beatSin, rangeWidth);
  uint8_t result = lowest + scaledBeat;
  return result;
}

inline int JlBeatSin(uint16_t beatsPerMinute, uint32_t elapsedTime, int lowest, int highest, uint8_t phaseOffset = 0) {
  uint8_t beat = JlBeat8(beatsPerMinute, elapsedTime);
  uint8_t beatSin = sin8(beat + phaseOffset);
  int rangeWidth = highest - lowest;
  int scaledBeat = rangeWidth * beatSin / 256;
  int result = lowest + scaledBeat;
  return result;
}

}  // namespace jazzlights

#endif  // JL_RENDER_MATH_H
