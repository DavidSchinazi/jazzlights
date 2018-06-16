#include "unisparks/util/color.hpp"
#include "unisparks/util/math.hpp"
#define FAST_HSL 1

namespace unisparks {

// This is an "accurate" version of color conversions using double prceision
// floating-points, should be pretty slow on ESP8266 chips
HslColor::HslColor(RgbColor color) {
  double in_r = color.red / 255.0;
  double in_g = color.green / 255.0;
  double in_b = color.blue / 255.0;
  double out_h, out_s, out_v;
  double min, max, delta;

  min = in_r < in_g ? in_r : in_g;
  min = min < in_b  ? min  : in_b;

  max = in_r > in_g ? in_r : in_g;
  max = max  > in_b ? max  : in_b;

  out_v = max;
  delta = max - min;
  if (delta < 0.00001) {
    out_s = 0;
    out_h = 0;
    goto ret;
  }
  if (max > 0.0) {  // NOTE: if Max is == 0, this divide would cause a crash
    out_s = (delta / max);
  } else {
    // if max is 0, then r = g = b = 0
    // s = 0, v is undefined
    out_s = 0.0;
    out_h = NAN; // its now undefined
    goto ret;
  }
  if (in_r >= max) { // > is bogus, just keeps compilor happy
    out_h = (in_g - in_b) / delta;        // between yellow & magenta
  } else if (in_g >= max) {
    out_h = 2.0 + (in_b - in_r) / delta;  // between cyan & yellow
  } else {
    out_h = 4.0 + (in_r - in_g) / delta;  // between magenta & cyan
  }

  out_h *= 60.0; // degrees

  if (out_h < 0.0) {
    out_h += 360.0;
  }

ret:
  hue = 255 * out_h / 360.0;
  saturation = 255 * out_s;
  lightness = 255 * out_v;
}

RgbColor::RgbColor(HslColor color) {
#ifndef FAST_HSL
  uint8_t h  = color.hue;
  uint8_t s = color.saturation;
  uint8_t v = color.lightness;

  double in_h = 360 * h / 255.0, in_s = s / 255.0, in_v = v / 255.0;
  double      hh, p, q, t, ff;
  long        i;
  double out_r, out_g, out_b;

  if (in_s <= 0.0) {

    return rgb(255 * in_v, 255 * in_v, 255 * in_v);
  }
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

  red = 255 * out_r;
  green = 255 * out_g;
  blue = 255 * out_b;
#else
  uint8_t hsv_h  = color.hue;
  uint8_t hsv_s = color.saturation;
  uint8_t hsv_v = color.lightness;

  uint8_t region, remainder, p, q, t, rgb_r, rgb_g, rgb_b;

  if (hsv_s == 0) {
    red = green = blue = hsv_v;
    return;
  }

  region = hsv_h / 43;
  remainder = (hsv_h - (region * 43)) * 6;

  p = (hsv_v * (255 - hsv_s)) >> 8;
  q = (hsv_v * (255 - ((hsv_s * remainder) >> 8))) >> 8;
  t = (hsv_v * (255 - ((hsv_s * (255 - remainder)) >> 8))) >> 8;

  switch (region) {
  case 0:
    rgb_r = hsv_v; rgb_g = t; rgb_b = p;
    break;
  case 1:
    rgb_r = q; rgb_g = hsv_v; rgb_b = p;
    break;
  case 2:
    rgb_r = p; rgb_g = hsv_v; rgb_b = t;
    break;
  case 3:
    rgb_r = p; rgb_g = q; rgb_b = hsv_v;
    break;
  case 4:
    rgb_r = t; rgb_g = p; rgb_b = hsv_v;
    break;
  default:
    rgb_r = hsv_v; rgb_g = p; rgb_b = q;
    break;
  }

  red = rgb_r;
  green = rgb_g;
  blue = rgb_b;
#endif
}

RgbaColor::RgbaColor(HslColor c) : RgbColor(c) {
}

} // namespace unisparks
