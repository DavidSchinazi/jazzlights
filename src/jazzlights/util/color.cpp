#include "jazzlights/util/color.h"

#include "jazzlights/util/math.h"

namespace jazzlights {

Color RgbColorFromHsv(uint8_t h, uint8_t s, uint8_t v) {
#ifdef ARDUINO
  CHSV hsv(h, s, v);
  CRGB rgb;
  hsv2rgb_rainbow(hsv, rgb);
  return Color(rgb.r, rgb.g, rgb.b);
#else   // ARDUINO
  uint8_t region, remainder, p, q, t, rgb_r, rgb_g, rgb_b;

  if (s == 0) { return Color(v, v, v); }

  region = h / 43;
  remainder = (h - (region * 43)) * 6;

  p = (v * (255 - s)) >> 8;
  q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

  switch (region) {
    case 0:
      rgb_r = v;
      rgb_g = t;
      rgb_b = p;
      break;
    case 1:
      rgb_r = q;
      rgb_g = v;
      rgb_b = p;
      break;
    case 2:
      rgb_r = p;
      rgb_g = v;
      rgb_b = t;
      break;
    case 3:
      rgb_r = p;
      rgb_g = q;
      rgb_b = v;
      break;
    case 4:
      rgb_r = t;
      rgb_g = p;
      rgb_b = v;
      break;
    default:
      rgb_r = v;
      rgb_g = p;
      rgb_b = q;
      break;
  }
  return Color(rgb_r, rgb_g, rgb_b);
#endif  // ARDUINO
}

}  // namespace jazzlights
