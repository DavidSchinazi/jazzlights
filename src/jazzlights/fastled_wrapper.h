#ifndef JAZZLIGHTS_FASTLED_WRAPPER_H
#define JAZZLIGHTS_FASTLED_WRAPPER_H

#include "jazzlights/config.h"

#if WEARABLE

// Fixes flickering <https://github.com/FastLED/FastLED/issues/306>.
#define FASTLED_ALLOW_INTERRUPTS 0

// Silences FastLED pragmas <https://github.com/FastLED/FastLED/issues/363>.
#define FASTLED_INTERNAL 1

#ifdef ESP8266
// Required to map feather huzzah and LoLin nodecmu pins properly.
#define FASTLED_ESP8266_RAW_PIN_ORDER
#endif  // ESP8266

#include <FastLED.h>

#else  // WEARABLE

// Copy and/or reimplement enough of FastLED to allow working on platforms that don't support FastLED.

#include <cstdint>

#define FL_PROGMEM
#define FL_PGM_READ_BYTE_NEAR(x) (*(x))

struct CRGB {
  union {
    struct {
      union {
        uint8_t r;
        uint8_t red;
      };
      union {
        uint8_t g;
        uint8_t green;
      };
      union {
        uint8_t b;
        uint8_t blue;
      };
    };
    uint8_t raw[3];
  };

  inline CRGB(uint32_t colorcode) __attribute__((always_inline))
  : r((colorcode >> 16) & 0xFF), g((colorcode >> 8) & 0xFF), b((colorcode >> 0) & 0xFF) {}

  enum KnownColors : uint32_t {
    AliceBlue = 0xF0F8FF,
    Amethyst = 0x9966CC,
    AntiqueWhite = 0xFAEBD7,
    Aqua = 0x00FFFF,
    Aquamarine = 0x7FFFD4,
    Azure = 0xF0FFFF,
    Beige = 0xF5F5DC,
    Bisque = 0xFFE4C4,
    Black = 0x000000,
    BlanchedAlmond = 0xFFEBCD,
    Blue = 0x0000FF,
    BlueViolet = 0x8A2BE2,
    Brown = 0xA52A2A,
    BurlyWood = 0xDEB887,
    CadetBlue = 0x5F9EA0,
    Chartreuse = 0x7FFF00,
    Chocolate = 0xD2691E,
    Coral = 0xFF7F50,
    CornflowerBlue = 0x6495ED,
    Cornsilk = 0xFFF8DC,
    Crimson = 0xDC143C,
    Cyan = 0x00FFFF,
    DarkBlue = 0x00008B,
    DarkCyan = 0x008B8B,
    DarkGoldenrod = 0xB8860B,
    DarkGray = 0xA9A9A9,
    DarkGrey = 0xA9A9A9,
    DarkGreen = 0x006400,
    DarkKhaki = 0xBDB76B,
    DarkMagenta = 0x8B008B,
    DarkOliveGreen = 0x556B2F,
    DarkOrange = 0xFF8C00,
    DarkOrchid = 0x9932CC,
    DarkRed = 0x8B0000,
    DarkSalmon = 0xE9967A,
    DarkSeaGreen = 0x8FBC8F,
    DarkSlateBlue = 0x483D8B,
    DarkSlateGray = 0x2F4F4F,
    DarkSlateGrey = 0x2F4F4F,
    DarkTurquoise = 0x00CED1,
    DarkViolet = 0x9400D3,
    DeepPink = 0xFF1493,
    DeepSkyBlue = 0x00BFFF,
    DimGray = 0x696969,
    DimGrey = 0x696969,
    DodgerBlue = 0x1E90FF,
    FireBrick = 0xB22222,
    FloralWhite = 0xFFFAF0,
    ForestGreen = 0x228B22,
    Fuchsia = 0xFF00FF,
    Gainsboro = 0xDCDCDC,
    GhostWhite = 0xF8F8FF,
    Gold = 0xFFD700,
    Goldenrod = 0xDAA520,
    Gray = 0x808080,
    Grey = 0x808080,
    Green = 0x008000,
    GreenYellow = 0xADFF2F,
    Honeydew = 0xF0FFF0,
    HotPink = 0xFF69B4,
    IndianRed = 0xCD5C5C,
    Indigo = 0x4B0082,
    Ivory = 0xFFFFF0,
    Khaki = 0xF0E68C,
    Lavender = 0xE6E6FA,
    LavenderBlush = 0xFFF0F5,
    LawnGreen = 0x7CFC00,
    LemonChiffon = 0xFFFACD,
    LightBlue = 0xADD8E6,
    LightCoral = 0xF08080,
    LightCyan = 0xE0FFFF,
    LightGoldenrodYellow = 0xFAFAD2,
    LightGreen = 0x90EE90,
    LightGrey = 0xD3D3D3,
    LightPink = 0xFFB6C1,
    LightSalmon = 0xFFA07A,
    LightSeaGreen = 0x20B2AA,
    LightSkyBlue = 0x87CEFA,
    LightSlateGray = 0x778899,
    LightSlateGrey = 0x778899,
    LightSteelBlue = 0xB0C4DE,
    LightYellow = 0xFFFFE0,
    Lime = 0x00FF00,
    LimeGreen = 0x32CD32,
    Linen = 0xFAF0E6,
    Magenta = 0xFF00FF,
    Maroon = 0x800000,
    MediumAquamarine = 0x66CDAA,
    MediumBlue = 0x0000CD,
    MediumOrchid = 0xBA55D3,
    MediumPurple = 0x9370DB,
    MediumSeaGreen = 0x3CB371,
    MediumSlateBlue = 0x7B68EE,
    MediumSpringGreen = 0x00FA9A,
    MediumTurquoise = 0x48D1CC,
    MediumVioletRed = 0xC71585,
    MidnightBlue = 0x191970,
    MintCream = 0xF5FFFA,
    MistyRose = 0xFFE4E1,
    Moccasin = 0xFFE4B5,
    NavajoWhite = 0xFFDEAD,
    Navy = 0x000080,
    OldLace = 0xFDF5E6,
    Olive = 0x808000,
    OliveDrab = 0x6B8E23,
    Orange = 0xFFA500,
    OrangeRed = 0xFF4500,
    Orchid = 0xDA70D6,
    PaleGoldenrod = 0xEEE8AA,
    PaleGreen = 0x98FB98,
    PaleTurquoise = 0xAFEEEE,
    PaleVioletRed = 0xDB7093,
    PapayaWhip = 0xFFEFD5,
    PeachPuff = 0xFFDAB9,
    Peru = 0xCD853F,
    Pink = 0xFFC0CB,
    Plaid = 0xCC5533,
    Plum = 0xDDA0DD,
    PowderBlue = 0xB0E0E6,
    Purple = 0x800080,
    Red = 0xFF0000,
    RosyBrown = 0xBC8F8F,
    RoyalBlue = 0x4169E1,
    SaddleBrown = 0x8B4513,
    Salmon = 0xFA8072,
    SandyBrown = 0xF4A460,
    SeaGreen = 0x2E8B57,
    Seashell = 0xFFF5EE,
    Sienna = 0xA0522D,
    Silver = 0xC0C0C0,
    SkyBlue = 0x87CEEB,
    SlateBlue = 0x6A5ACD,
    SlateGray = 0x708090,
    SlateGrey = 0x708090,
    Snow = 0xFFFAFA,
    SpringGreen = 0x00FF7F,
    SteelBlue = 0x4682B4,
    Tan = 0xD2B48C,
    Teal = 0x008080,
    Thistle = 0xD8BFD8,
    Tomato = 0xFF6347,
    Turquoise = 0x40E0D0,
    Violet = 0xEE82EE,
    Wheat = 0xF5DEB3,
    White = 0xFFFFFF,
    WhiteSmoke = 0xF5F5F5,
    Yellow = 0xFFFF00,
    YellowGreen = 0x9ACD32,

    // LED RGB color that roughly approximates
    // the color of incandescent fairy lights,
    // assuming that you're using FastLED
    // color correction on your LEDs (recommended).
    FairyLight = 0xFFE42D,
    // If you are using no color correction, use this
    FairyLightNCC = 0xFF9D2A
  };
};

typedef uint32_t TProgmemRGBPalette16[16];
extern const TProgmemRGBPalette16 CloudColors_p FL_PROGMEM;
extern const TProgmemRGBPalette16 OceanColors_p FL_PROGMEM;
extern const TProgmemRGBPalette16 ForestColors_p FL_PROGMEM;
extern const TProgmemRGBPalette16 RainbowColors_p FL_PROGMEM;
extern const TProgmemRGBPalette16 PartyColors_p FL_PROGMEM;
extern const TProgmemRGBPalette16 HeatColors_p FL_PROGMEM;

static const uint8_t b_m16_interleave[] = {0, 49, 49, 41, 90, 27, 117, 10};

inline int absi(int i) { return i < 0 ? -i : i; }

inline uint8_t qmul8(uint8_t i, uint8_t j) {
  int p = ((int)i * (int)(j));
  if (p > 255) { p = 255; }
  return p;
}

inline uint8_t scale8(uint8_t i, uint8_t scale) { return ((uint16_t)i * (uint16_t)(scale)) >> 8; }

inline uint8_t scale8_video(uint8_t i, uint8_t scale) {
  uint8_t j = (((int)i * (int)scale) >> 8) + ((i && scale) ? 1 : 0);
  return j;
}

inline uint8_t sin8(uint8_t theta) {
  uint8_t offset = theta;
  if (theta & 0x40) { offset = (uint8_t)255 - offset; }
  offset &= 0x3F;  // 0..63

  uint8_t secoffset = offset & 0x0F;  // 0..15
  if (theta & 0x40) { secoffset++; }

  uint8_t section = offset >> 4;  // 0..3
  uint8_t s2 = section * 2;
  const uint8_t* p = b_m16_interleave;
  p += s2;
  uint8_t b = *p;
  p++;
  uint8_t m16 = *p;

  uint8_t mx = (m16 * secoffset) >> 4;

  int8_t y = mx + b;
  if (theta & 0x80) { y = -y; }

  y += 128;

  return y;
}

inline uint8_t cos8(uint8_t theta) { return sin8(theta + 64); }

inline uint8_t triwave8(uint8_t in) {
  if (in & 0x80) { in = 255 - in; }
  uint8_t out = in << 1;
  return out;
}

inline uint8_t ease8InOutQuad(uint8_t i) {
  uint8_t j = i;
  if (j & 0x80) { j = 255 - j; }
  uint8_t jj = scale8(j, (j + 1));
  uint8_t jj2 = jj << 1;
  if (i & 0x80) { jj2 = 255 - jj2; }
  return jj2;
}

inline uint8_t quadwave8(uint8_t in) { return ease8InOutQuad(triwave8(in)); }

inline uint8_t qadd8(uint8_t i, uint8_t j) {
  unsigned int t = i + j;
  if (t > 255) { t = 255; }
  return t;
}

inline int8_t qadd7(int8_t i, int8_t j) {
  int16_t t = i + j;
  if (t > 127) { t = 127; }
  return t;
}

inline uint8_t qsub8(uint8_t i, uint8_t j) {
  int t = i - j;
  if (t < 0) { t = 0; }
  return t;
}

constexpr inline int8_t abs8(int8_t i) { return i >= 0 ? i : -i; }

constexpr inline int8_t avg7(int8_t i, int8_t j) { return ((i + j) >> 1) + (i & 0x1); }

constexpr inline int16_t avg15(int16_t i, int16_t j) {
  return ((int32_t)((int32_t)(i) + (int32_t)(j)) >> 1) + (i & 0x1);
}

constexpr inline uint16_t scale16(uint16_t i, uint16_t scale) {
  return ((uint32_t)(i) * (1 + (uint32_t)(scale))) / 65536;
}

inline uint8_t lerp8by8(uint8_t a, uint8_t b, uint8_t frac) {
  if (b > a) {
    return a + scale8(b - a, frac);
  } else {
    return a - scale8(a - b, frac);
  }
}

inline int16_t lerp15by16(int16_t a, int16_t b, uint16_t frac) {
  if (b > a) {
    return a + scale16(b - a, frac);
  } else {
    return a - scale16(a - b, frac);
  }
}

#endif  // WEARABLE

// Define our own due to <https://github.com/FastLED/FastLED/issues/1435>.
extern const TProgmemRGBPalette16 JLLavaColors_p FL_PROGMEM;

#endif  // JAZZLIGHTS_FASTLED_WRAPPER_H
