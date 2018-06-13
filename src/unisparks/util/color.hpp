#ifndef UNISPARKS_COLOR_H
#define UNISPARKS_COLOR_H
#include <stdint.h>
#include <string.h>

namespace unisparks {

struct HslColor;
struct RgbColor;
struct RgbaColor;

struct RgbColor {
  constexpr RgbColor()
    : red(0), green(0), blue(0) {}
  constexpr RgbColor(uint8_t r, uint8_t g, uint8_t b)
    : red(r), green(g), blue(b) {}
  constexpr RgbColor(uint32_t c)
    : red((c >> 16) & 0xFF), green((c >> 8) & 0xFF), blue(c & 0xFF) {}
  RgbColor(HslColor c);

  constexpr bool operator==(const RgbColor& other) const {
    return other.red == red &&
           other.green == green &&
           other.blue == blue;
  }

  constexpr bool operator!=(const RgbColor& other) const {
    return !(*this == other);
  }

  uint8_t red;
  uint8_t green;
  uint8_t blue;
};


struct RgbaColor : RgbColor {
  constexpr RgbaColor()
    : RgbColor(), alpha(0xFF) {}
  constexpr RgbaColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xFF)
    : RgbColor(r, g, b), alpha(a) {}
  constexpr RgbaColor(RgbColor c, uint8_t a = 0xFF)
    : RgbColor(c), alpha(a) {}
  constexpr RgbaColor(uint32_t c)
    : RgbColor(c), alpha(0xFF - ((c >> 24) & 0xFF)) {}
  RgbaColor(HslColor c);
  constexpr bool operator==(const RgbaColor& other) const {
    return static_cast<const RgbColor&>(*this) == other && other.alpha == alpha;
  }

  constexpr bool operator!=(const RgbaColor& other) const {
    return !(*this == other);
  }

  uint8_t alpha;
};

inline RgbColor blend(RgbaColor fg, RgbaColor bg) {
  unsigned int alpha = fg.alpha + 1;
  unsigned int inv_alpha = 256 - fg.alpha;
  return {
    static_cast<uint8_t>((alpha * fg.red + inv_alpha * bg.red) >> 8),
    static_cast<uint8_t>((alpha * fg.green + inv_alpha * bg.green) >> 8),
    static_cast<uint8_t>((alpha * fg.blue + inv_alpha * bg.blue) >> 8)
  };
}

struct HslColor {
  constexpr HslColor() : hue(0), saturation(0), lightness(0) {}
  constexpr HslColor(uint8_t h, uint8_t s, uint8_t l)
    : hue(h), saturation(s), lightness(l) {}
  HslColor(RgbColor c);

  constexpr bool operator==(const HslColor& other) const {
    return other.hue == hue &&
           other.saturation == saturation &&
           other.lightness == lightness;
  }

  uint8_t hue;
  uint8_t saturation;
  uint8_t lightness;

  static constexpr uint8_t HUE_RED = 0;
  static constexpr uint8_t HUE_ORANGE = 32;
  static constexpr uint8_t HUE_YELLOW = 64;
  static constexpr uint8_t HUE_GREEN = 96;
  static constexpr uint8_t HUE_AQUA = 128;
  static constexpr uint8_t HUE_BLUE = 160;
  static constexpr uint8_t HUE_PURPLE = 192;
  static constexpr uint8_t HUE_PINK = 224;
};

class Color {
 public:
  enum Space {
    RGBA,
    HSL
  };

  constexpr Color()
    : space_(RGBA), rgba_(RgbaColor()) {}
  constexpr Color(uint32_t c)
    : space_(RGBA), rgba_(RgbaColor(c)) {}
  constexpr Color(const RgbaColor& c)
    : space_(RGBA), rgba_(c) {}
  constexpr Color(const RgbColor& c)
    : space_(RGBA), rgba_(c) {}
  constexpr Color(const HslColor& c)
    : space_(HSL), hsl_(c) {}

  constexpr static Color rbg(uint32_t v) {
    return Color(v);
  }
  constexpr static Color rbg(uint8_t r, uint8_t g, uint8_t b) {
    return Color(RgbColor(r, g, b));
  }
  constexpr static Color rbga(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return Color(RgbaColor(r, g, b, a));
  }
  constexpr static Color hsl(uint8_t h, uint8_t s, uint8_t l) {
    return Color(HslColor(h, s, l));
  }

  constexpr bool operator==(const Color& other) const {
    return space_ == other.space_ &&
           ((space_ == RGBA && rgba_ == other.rgba_) ||
            (space_ == HSL && hsl_ == other.hsl_));
  }

  constexpr bool operator!=(const Color& other) const {
    return !(*this == other);
  }

  Color& operator=(const Color&) = default;

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
    return blend(asRgba(), RgbaColor(0x0));
  }

  constexpr RgbaColor asRgba() const {
    return space_ == RGBA ? rgba_ : RgbaColor(hsl_);
  }

  constexpr HslColor asHsl() const {
    return space_ == RGBA ? HslColor(rgba_) : hsl_;
  }

  RgbColor& convertToRgba() {
    *this = asRgba();
    return rgba_;
  }

  HslColor& convertToHsl() {
    *this = asHsl();
    return hsl_;
  }

  RgbaColor asRgbAlphaLightness() const {
    if (space_ == RGBA) {
      return rgba_;
    }
    //assert(space_ == HSL);
    RgbaColor c = asRgba();
    c.alpha = hsl_.lightness;
    return c;
  }


 private:
  Space space_;
  union {
    RgbaColor rgba_;
    HslColor hsl_;
  };
};

static constexpr Color TRANSPARENT = RgbaColor(0, 0, 0, 0);
static constexpr Color BLACK = Color(0x0);
static constexpr Color RED = Color(0xff0000);
static constexpr Color GREEN = Color(0x00ff00);
static constexpr Color PURPLE = Color(0x9C27B0);
static constexpr Color CYAN = Color(0x00BCD4);
static constexpr Color YELLOW = Color(0xFFFF00);

inline Color alphaBlend(Color fg, Color bg) {
  return blend(fg.asRgba(), bg.asRgba());
}

inline Color alphaLightnessBlend(Color fg, Color bg) {
  return blend(fg.asRgbAlphaLightness(), bg.asRgba());
}

inline Color blackMask(Color fg, Color bg) {
  return fg != BLACK ? fg : bg;
}

// With this: 15-16 fps on complex overlays
// using Color = RgbaColor;

} // namespace unisparks

#ifndef ARDUINO
#include <iostream>
namespace unisparks {

inline std::ostream& operator<<(std::ostream& out, const RgbColor& c) {
  out << "RGB(" << int(c.red) << "," << int(c.green) << "," << int(
        c.blue) << ")";
  return out;
}

inline std::ostream& operator<<(std::ostream& out, const RgbaColor& c) {
  out << "RGBa(" << int(c.red) << "," << int(c.green) << "," << int(
        c.blue) << "," << int(c.alpha) << ")";
  return out;
}

inline std::ostream& operator<<(std::ostream& out, const HslColor& c) {
  out << "HSL(" << int(c.hue) << "," << int(c.saturation) << "," << int(
        c.lightness) << ")";
  return out;
}

} // namespace unisparks
#endif /* ifndef ARDUINO */
#endif /* UNISPARKS_COLOR_H */
