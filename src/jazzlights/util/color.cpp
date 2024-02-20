#include "jazzlights/util/color.h"

#include "jazzlights/util/math.h"
#define FAST_HSL 1

namespace jazzlights {

RgbColor RgbColorFromHsv(uint8_t h, uint8_t s, uint8_t v) {
#ifndef FAST_HSL
  double in_h = 360 * h / 255.0, in_s = s / 255.0, in_v = v / 255.0;
  double hh, p, q, t, ff;
  long i;
  double out_r, out_g, out_b;

  if (in_s <= 0.0) { return RgbColor(255 * in_v, 255 * in_v, 255 * in_v); }
  hh = in_h;
  if (hh >= 360.0) { hh = 0.0; }
  hh /= 60.0;
  i = (long)hh;
  ff = hh - i;
  p = in_v * (1.0 - in_s);
  q = in_v * (1.0 - (in_s * ff));
  t = in_v * (1.0 - (in_s * (1.0 - ff)));

  switch (i) {
    case 0:
      out_r = in_v;
      out_g = t;
      out_b = p;
      break;
    case 1:
      out_r = q;
      out_g = in_v;
      out_b = p;
      break;
    case 2:
      out_r = p;
      out_g = in_v;
      out_b = t;
      break;

    case 3:
      out_r = p;
      out_g = q;
      out_b = in_v;
      break;
    case 4:
      out_r = t;
      out_g = p;
      out_b = in_v;
      break;
    case 5:
    default:
      out_r = in_v;
      out_g = p;
      out_b = q;
      break;
  }

  return RgbColor(255 * out_r, 255 * out_g, 255 * out_b);
#else
  uint8_t region, remainder, p, q, t, rgb_r, rgb_g, rgb_b;

  if (s == 0) { return RgbColor(v, v, v); }

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
  return RgbColor(rgb_r, rgb_g, rgb_b);
#endif
}

}  // namespace jazzlights
