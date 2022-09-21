#ifndef JAZZLIGHTS_MATH_H
#define JAZZLIGHTS_MATH_H
#define _USE_MATH_DEFINES
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "jazzlights/fastled_wrapper.h"

namespace jazzlights {

template <class T>
constexpr const T& min(const T& a, const T& b) {
  return (b < a) ? b : a;
}
template <class T>
constexpr const T& max(const T& a, const T& b) {
  return (a < b) ? b : a;
}
template <class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
  return (v < lo) ? lo : (hi < v) ? hi : v;
}

template <class T>
constexpr T square(T x) {
  return x * x;
}

/**
 * Returns positive reminder
 */
inline double amod(double a, double b) { return fabs(fmod(a, b)); }

/**
 * Returns triangle wave with the period of 1 and amplitude of 1
 */
inline double triwave(double x) {
  // 1 - 2*abs[mod[2*x+0.5, 2]- 1]
  return 1 - 2 * fabs(amod(2 * x + 0.5, 2) - 1);
}

namespace internal {

extern uint8_t sqrt16(uint16_t x);

/**
 * Online calculation of median, variance, and standard deviation
 */
class Welford {
 public:
  void reset() {
    n_ = 0;
    mean_ = m2_ = 0;
  }
  void add(double x) {
    n_++;
    double delta = x - mean_;
    mean_ += delta / n_;
    m2_ += delta * (x - mean_);
  }

  double mean() const { return mean_; }
  double variance() const { return n_ < 2 ? NAN : m2_ / (n_ - 1); }
  double stddev() const { return sqrt(variance()); }

 private:
  int n_;
  double mean_;
  double m2_;
};

}  // namespace internal

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

#endif  // JAZZLIGHTS_PROTO_H
