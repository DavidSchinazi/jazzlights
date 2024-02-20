#ifndef JL_UTIL_COLOR_H
#define JL_UTIL_COLOR_H

#include <stdint.h>
#include <string.h>

#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/util/math.h"

namespace jazzlights {

struct HslColor;
struct RgbColor;
struct RgbaColor;

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

  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

struct RgbaColor : RgbColor {
  constexpr RgbaColor() : RgbColor(), alpha(0xFF) {}
  constexpr RgbaColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xFF) : RgbColor(r, g, b), alpha(a) {}
  constexpr RgbaColor(RgbColor c, uint8_t a = 0xFF) : RgbColor(c), alpha(a) {}
  constexpr RgbaColor(uint32_t c) : RgbColor(c), alpha(0xFF - ((c >> 24) & 0xFF)) {}
  RgbaColor(HslColor c);
  constexpr bool operator==(const RgbaColor& other) const {
    return static_cast<const RgbColor&>(*this) == static_cast<const RgbColor&>(other) && other.alpha == alpha;
  }

  constexpr bool operator!=(const RgbaColor& other) const { return !(*this == other); }

  uint8_t alpha;
};

struct HslColor {
  constexpr HslColor() : hue(0), saturation(0), lightness(0) {}
  constexpr HslColor(uint8_t h, uint8_t s, uint8_t l) : hue(h), saturation(s), lightness(l) {}
  HslColor(RgbColor c);

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

  constexpr Color() : space_(RGBA), rgba_(RgbaColor()) {}
  constexpr Color(uint32_t c) : space_(RGBA), rgba_(RgbaColor(c)) {}
  constexpr Color(const RgbaColor& c) : space_(RGBA), rgba_(c) {}
  constexpr Color(const RgbColor& c) : space_(RGBA), rgba_(c) {}
  constexpr Color(const HslColor& c) : space_(HSL), hsl_(c) {}

  constexpr static Color rgb(uint32_t v) { return Color(v); }
  constexpr static Color rgb(uint8_t r, uint8_t g, uint8_t b) { return Color(RgbColor(r, g, b)); }
  constexpr static Color rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { return Color(RgbaColor(r, g, b, a)); }
  constexpr static Color hsl(uint8_t h, uint8_t s, uint8_t l) { return Color(HslColor(h, s, l)); }

  constexpr bool operator==(const Color& other) const {
    return space_ == other.space_ &&
           ((space_ == RGBA && rgba_ == other.rgba_) || (space_ == HSL && hsl_ == other.hsl_));
  }

  constexpr bool operator!=(const Color& other) const { return !(*this == other); }

  Color& operator=(const RgbaColor& other) {
    space_ = RGBA;
    rgba_ = other;
    return *this;
  }

  Color& operator=(const HslColor& other) {
    space_ = HSL;
    hsl_ = other;
    return *this;
  }

  RgbColor asRgb() const {
    const RgbaColor rgba = asRgba();
    return RgbColor(rgba.red, rgba.green, rgba.blue);
  }

  constexpr RgbaColor asRgba() const { return space_ == RGBA ? rgba_ : RgbaColor(hsl_); }

  constexpr HslColor asHsl() const { return space_ == RGBA ? HslColor(rgba_) : hsl_; }

  RgbColor& convertToRgba() {
    *this = asRgba();
    return rgba_;
  }

  HslColor& convertToHsl() {
    *this = asHsl();
    return hsl_;
  }

 private:
  Space space_;
  union {
    RgbaColor rgba_;
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

inline RgbColor nscale8(RgbColor rgb, uint8_t scale) {
  if (scale == 255) { return rgb; }
  const uint16_t scale_fixed = scale + 1;
  rgb.red = (((uint16_t)rgb.red) * scale_fixed) >> 8;
  rgb.green = (((uint16_t)rgb.green) * scale_fixed) >> 8;
  rgb.blue = (((uint16_t)rgb.blue) * scale_fixed) >> 8;
  return rgb;
}

// With this: 15-16 fps on complex overlays
// using Color = RgbaColor;

}  // namespace jazzlights

#ifndef ARDUINO
#include <iostream>
namespace jazzlights {

inline std::ostream& operator<<(std::ostream& out, const RgbColor& c) {
  out << "RGB(" << int(c.red) << "," << int(c.green) << "," << int(c.blue) << ")";
  return out;
}

inline std::ostream& operator<<(std::ostream& out, const RgbaColor& c) {
  out << "RGBa(" << int(c.red) << "," << int(c.green) << "," << int(c.blue) << "," << int(c.alpha) << ")";
  return out;
}

inline std::ostream& operator<<(std::ostream& out, const HslColor& c) {
  out << "HSL(" << int(c.hue) << "," << int(c.saturation) << "," << int(c.lightness) << ")";
  return out;
}

}  // namespace jazzlights

#endif  // ARDUINO
#endif  // JL_UTIL_COLOR_H
