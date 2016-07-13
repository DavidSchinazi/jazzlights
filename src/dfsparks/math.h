#ifndef DFSPARKS_MATH_H
#define DFSPARKS_MATH_H
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

namespace dfsparks {

static const uint8_t b_m16_interleave[] = {0, 49, 49, 41, 90, 27, 117, 10};

inline int square(int x) { return x * x; }

/// take abs() of a signed 8-bit uint8_t
inline int absi(int i) { return i < 0 ? -i : i; }

/// saturating 8x8 bit multiplication, with 8 bit result
/// @returns the product of i * j, capping at 0xFF
inline uint8_t qmul8(uint8_t i, uint8_t j) {
  int p = ((int)i * (int)(j));
  if (p > 255)
    p = 255;
  return p;
}

///  scale one byte by a second one, which is treated as
///  the numerator of a fraction whose denominator is 256
///  In other words, it computes i * (scale / 256)
///  4 clocks AVR with MUL, 2 clocks ARM
inline uint8_t scale8(uint8_t i, uint8_t scale) {
  return ((uint16_t)i * (uint16_t)(scale)) >> 8;
}

///  The "video" version of scale8 guarantees that the output will
///  be only be zero if one or both of the inputs are zero.  If both
///  inputs are non-zero, the output is guaranteed to be non-zero.
///  This makes for better 'video'/LED dimming, at the cost of
///  several additional cycles.
inline uint8_t scale8_video(uint8_t i, uint8_t scale) {
  uint8_t j = (((int)i * (int)scale) >> 8) + ((i && scale) ? 1 : 0);
  return j;
}

/// Fast 8-bit approximation of sin(x). This approximation never varies more
/// than
/// 2% from the floating point value you'd get by doing
///
///     float s = (sin(x) * 128.0) + 128;
///
/// @param theta input angle from 0-255
/// @returns sin of theta, value between 0 and 255
inline uint8_t sin8(uint8_t theta) {
  uint8_t offset = theta;
  if (theta & 0x40) {
    offset = (uint8_t)255 - offset;
  }
  offset &= 0x3F; // 0..63

  uint8_t secoffset = offset & 0x0F; // 0..15
  if (theta & 0x40)
    secoffset++;

  uint8_t section = offset >> 4; // 0..3
  uint8_t s2 = section * 2;
  const uint8_t *p = b_m16_interleave;
  p += s2;
  uint8_t b = *p;
  p++;
  uint8_t m16 = *p;

  uint8_t mx = (m16 * secoffset) >> 4;

  int8_t y = mx + b;
  if (theta & 0x80)
    y = -y;

  y += 128;

  return y;
}

/// Fast 8-bit approximation of cos(x). This approximation never varies more
/// than
/// 2% from the floating point value you'd get by doing
///
///     float s = (cos(x) * 128.0) + 128;
///
/// @param theta input angle from 0-255
/// @returns sin of theta, value between 0 and 255
inline uint8_t cos8(uint8_t theta) { return sin8(theta + 64); }

inline uint8_t triwave8(uint8_t in) {
  if (in & 0x80) {
    in = 255 - in;
  }
  uint8_t out = in << 1;
  return out;
}

/// ease8InOutQuad: 8-bit quadratic ease-in / ease-out function
///                Takes around 13 cycles on AVR
inline uint8_t ease8InOutQuad(uint8_t i) {
  uint8_t j = i;
  if (j & 0x80) {
    j = 255 - j;
  }
  uint8_t jj = scale8(j, (j + 1));
  uint8_t jj2 = jj << 1;
  if (i & 0x80) {
    jj2 = 255 - jj2;
  }
  return jj2;
}

/// quadwave8: quadratic waveform generator.  Spends just a little more
///            time at the limits than 'sine' does.
inline uint8_t quadwave8(uint8_t in) { return ease8InOutQuad(triwave8(in)); }

/// add one byte to another, saturating at 0xFF
/// @param i - first byte to add
/// @param j - second byte to add
/// @returns the sum of i & j, capped at 0xFF
inline uint8_t qadd8(uint8_t i, uint8_t j) {
  unsigned int t = i + j;
  if (t > 255)
    t = 255;
  return t;
}

/// Add one byte to another, saturating at 0x7F
/// @param i - first byte to add
/// @param j - second byte to add
/// @returns the sum of i & j, capped at 0xFF
inline int8_t qadd7(int8_t i, int8_t j) {
  int16_t t = i + j;
  if (t > 127)
    t = 127;
  return t;
}

/// subtract one byte from another, saturating at 0x00
/// @returns i - j with a floor of 0
inline uint8_t qsub8(uint8_t i, uint8_t j) {
  int t = i - j;
  if (t < 0)
    t = 0;
  return t;
}

extern uint8_t random8();
extern uint8_t random8(uint8_t lim);
extern uint8_t random8(uint8_t min, uint8_t lim);
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

} // namespace dfsparks
#endif /* DFSPARKS_PROTO_H */
