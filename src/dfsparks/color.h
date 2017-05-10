#ifndef DFSPARKS_COLOR_H
#define DFSPARKS_COLOR_H
#include <stdint.h>
#include <string.h>

namespace dfsparks {

struct HslColor;
struct RgbaColor;

struct RgbaColor {
  RgbaColor() {}
  RgbaColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xFF)
      : red(r), green(g), blue(b), alpha(a) {}
  RgbaColor(uint32_t c)
      : red((c >> 16) & 0xFF), green((c >> 8) & 0xFF), blue(c & 0xFF),
        alpha(0xFF-((c >> 24) & 0xFF)) {}
  RgbaColor(HslColor c);

  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t alpha;
};

struct HslColor {
  HslColor() {}
  HslColor(uint8_t h, uint8_t s, uint8_t l) 
    : hue(h), saturation(s), lightness(l) {}
  HslColor(RgbaColor c);

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

} // namespace dfsparks
#endif /* DFSPARKS_COLOR_H */
