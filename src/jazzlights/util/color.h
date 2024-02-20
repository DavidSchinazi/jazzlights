#ifndef JL_UTIL_COLOR_H
#define JL_UTIL_COLOR_H

#include <stdint.h>
#include <string.h>

#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/util/math.h"

namespace jazzlights {

struct HslColor;
struct RgbColor;

inline void nscale8x3_video(uint8_t& r, uint8_t& g, uint8_t& b, uint8_t scale) {
  uint8_t nonzeroscale = (scale != 0) ? 1 : 0;
  r = (r == 0) ? 0 : (((int)r * (int)(scale)) >> 8) + nonzeroscale;
  g = (g == 0) ? 0 : (((int)g * (int)(scale)) >> 8) + nonzeroscale;
  b = (b == 0) ? 0 : (((int)b * (int)(scale)) >> 8) + nonzeroscale;
}

struct RgbColor {
  constexpr RgbColor() : red(0), green(0), blue(0) {}
  constexpr RgbColor(uint8_t r, uint8_t g, uint8_t b) : red(r), green(g), blue(b) {}
  constexpr RgbColor(uint32_t c) : red((c >> 16) & 0xFF), green((c >> 8) & 0xFF), blue(c & 0xFF) {}
  RgbColor(CRGB other) : RgbColor(other.r, other.g, other.b) {}
  RgbColor(HslColor c);

  constexpr bool operator==(const RgbColor& other) const {
    return other.red == red && other.green == green && other.blue == blue;
  }

  constexpr bool operator!=(const RgbColor& other) const { return !(*this == other); }

  RgbColor& operator+=(const RgbColor& other) {
    red = qadd8(red, other.red);
    green = qadd8(green, other.green);
    blue = qadd8(blue, other.blue);
    return *this;
  }

  RgbColor& operator%=(uint8_t scaledown) {
    nscale8x3_video(red, green, blue, scaledown);
    return *this;
  }

  RgbColor& nscale8(uint8_t scale) {
    if (scale == 255) { return *this; }
    const uint16_t scale_fixed = scale + 1;
    red = (((uint16_t)red) * scale_fixed) >> 8;
    green = (((uint16_t)green) * scale_fixed) >> 8;
    blue = (((uint16_t)blue) * scale_fixed) >> 8;
    return *this;
  }

  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

struct HslColor {
  constexpr HslColor() : hue(0), saturation(0), lightness(0) {}
  constexpr HslColor(uint8_t h, uint8_t s, uint8_t l) : hue(h), saturation(s), lightness(l) {}

  constexpr bool operator==(const HslColor& other) const {
    return other.hue == hue && other.saturation == saturation && other.lightness == lightness;
  }

  uint8_t hue;
  uint8_t saturation;
  uint8_t lightness;

  enum : uint8_t {
    HUE_RED = 0,
    HUE_ORANGE = 32,
    HUE_YELLOW = 64,
    HUE_GREEN = 96,
    HUE_AQUA = 128,
    HUE_BLUE = 160,
    HUE_PURPLE = 192,
    HUE_PINK = 224,
  };
};

class Color {
 public:
  enum Space { RGBA, HSL };

  constexpr Color() : space_(RGBA), rgba_(RgbColor()) {}
  constexpr Color(uint32_t c) : space_(RGBA), rgba_(RgbColor(c)) {}
  constexpr Color(const RgbColor& c) : space_(RGBA), rgba_(c) {}
  constexpr Color(const HslColor& c) : space_(HSL), hsl_(c) {}

  constexpr bool operator==(const Color& other) const {
    return space_ == other.space_ &&
           ((space_ == RGBA && rgba_ == other.rgba_) || (space_ == HSL && hsl_ == other.hsl_));
  }

  constexpr bool operator!=(const Color& other) const { return !(*this == other); }

  Color& operator=(const HslColor& other) {
    space_ = HSL;
    hsl_ = other;
    return *this;
  }

  constexpr RgbColor asRgb() const { return space_ == RGBA ? rgba_ : RgbColor(hsl_); }

 private:
  Space space_;
  union {
    RgbColor rgba_;
    HslColor hsl_;
  };
};

static constexpr Color Black() { return Color(0x0); }
static constexpr Color Red() { return Color(0xff0000); }
static constexpr Color Green() { return Color(0x00ff00); }
static constexpr Color Blue() { return Color(0x0000ff); }
static constexpr Color Purple() { return Color(0x9C27B0); }
static constexpr Color Cyan() { return Color(0x00BCD4); }
static constexpr Color Yellow() { return Color(0xFFFF00); }
static constexpr Color White() { return Color(0xffffff); }

// With this: 15-16 fps on complex overlays
// using Color = RgbColor;

}  // namespace jazzlights

#ifndef ARDUINO
#include <iostream>
namespace jazzlights {

inline std::ostream& operator<<(std::ostream& out, const RgbColor& c) {
  out << "RGB(" << int(c.red) << "," << int(c.green) << "," << int(c.blue) << ")";
  return out;
}

inline std::ostream& operator<<(std::ostream& out, const HslColor& c) {
  out << "HSL(" << int(c.hue) << "," << int(c.saturation) << "," << int(c.lightness) << ")";
  return out;
}

}  // namespace jazzlights

#endif  // ARDUINO
#endif  // JL_UTIL_COLOR_H
