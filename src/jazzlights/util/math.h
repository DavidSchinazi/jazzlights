#ifndef JL_MATH_H
#define JL_MATH_H
#define _USE_MATH_DEFINES
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "jazzlights/fastled_wrapper.h"

namespace jazzlights {

template <class T>
constexpr T square(T x) {
  return x * x;
}

// The jlbeat* functions are almost identical to the corresponding beat* functions in FastLED/src/lib8tion.h, except
// that they operate on elapsedTime (the time since the current pattern has started) instead of the absolute time.

inline uint16_t jlbeat88(uint16_t beats_per_minute_88, uint32_t elapsedTime) {
  return (elapsedTime * beats_per_minute_88 * 280) >> 16;
}

inline uint16_t jlbeat16(uint16_t beats_per_minute, uint32_t elapsedTime) {
  if (beats_per_minute < 256) beats_per_minute <<= 8;
  return jlbeat88(beats_per_minute, elapsedTime);
}

inline uint8_t jlbeat8(uint16_t beats_per_minute, uint32_t elapsedTime) {
  return jlbeat16(beats_per_minute, elapsedTime) >> 8;
}

inline uint8_t jlbeatsin8(uint16_t beats_per_minute, uint32_t elapsedTime, uint8_t lowest = 0, uint8_t highest = 255,
                          uint8_t phase_offset = 0) {
  uint8_t beat = jlbeat8(beats_per_minute, elapsedTime);
  uint8_t beatsin = sin8(beat + phase_offset);
  uint8_t rangewidth = highest - lowest;
  uint8_t scaledbeat = scale8(beatsin, rangewidth);
  uint8_t result = lowest + scaledbeat;
  return result;
}

inline int jlbeatsin(uint16_t beats_per_minute, uint32_t elapsedTime, int lowest, int highest,
                     uint8_t phase_offset = 0) {
  uint8_t beat = jlbeat8(beats_per_minute, elapsedTime);
  uint8_t beatsin = sin8(beat + phase_offset);
  int rangewidth = highest - lowest;
  int scaledbeat = rangewidth * beatsin / 256;
  int result = lowest + scaledbeat;
  return result;
}

}  // namespace jazzlights

#endif  // JL_PROTO_H
