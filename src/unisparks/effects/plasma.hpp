#ifndef UNISPARKS_EFFECTS_PLASMA_H
#define UNISPARKS_EFFECTS_PLASMA_H
#include "unisparks/config.h"
#include "unisparks/effects/functional.hpp"
#include "unisparks/util/math.hpp"

#if WEARABLE
#  define PLASMA_SPEED 30
#else //WEARABLE
#  define PLASMA_SPEED 10
#endif //WEARABLE

namespace unisparks {

auto plasma = []() {
  return effect([](const Frame & frame) {
    using internal::sin8;
    using internal::cos8;

    constexpr int32_t speed = PLASMA_SPEED;
    uint8_t offset = speed * frame.time / 255;
    int plasVector = offset * 16;

    // Calculate current center of plasma pattern (can be offscreen)
    int xOffset = cos8(plasVector / 256);
    int yOffset = sin8(plasVector / 256);

    return [ = ](const Pixel& px) -> Color {
      double xx = 16.0 * px.coord.x / width(px.frame);
      double yy = 16.0 * px.coord.y / height(px.frame);
      uint8_t hue = sin8(sqrt(square((xx - 7.5) * 10 + xOffset - 127) +
      square((yy - 2) * 10 + yOffset - 127)) +
      offset);

      return HslColor(hue, 255, 255);
    };
  });
};

// Most of the code here comes from FastLED
// https://github.com/FastLED/FastLED

#define FL_PROGMEM
typedef uint32_t TProgmemRGBPalette16[16];

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

  inline CRGB( uint32_t colorcode)  __attribute__((always_inline))
    : r((colorcode >> 16) & 0xFF), g((colorcode >> 8) & 0xFF), b((colorcode >> 0) & 0xFF)
    {
    }

  enum KnownColors : uint32_t {
    AliceBlue=0xF0F8FF,
    Amethyst=0x9966CC,
    AntiqueWhite=0xFAEBD7,
    Aqua=0x00FFFF,
    Aquamarine=0x7FFFD4,
    Azure=0xF0FFFF,
    Beige=0xF5F5DC,
    Bisque=0xFFE4C4,
    Black=0x000000,
    BlanchedAlmond=0xFFEBCD,
    Blue=0x0000FF,
    BlueViolet=0x8A2BE2,
    Brown=0xA52A2A,
    BurlyWood=0xDEB887,
    CadetBlue=0x5F9EA0,
    Chartreuse=0x7FFF00,
    Chocolate=0xD2691E,
    Coral=0xFF7F50,
    CornflowerBlue=0x6495ED,
    Cornsilk=0xFFF8DC,
    Crimson=0xDC143C,
    Cyan=0x00FFFF,
    DarkBlue=0x00008B,
    DarkCyan=0x008B8B,
    DarkGoldenrod=0xB8860B,
    DarkGray=0xA9A9A9,
    DarkGrey=0xA9A9A9,
    DarkGreen=0x006400,
    DarkKhaki=0xBDB76B,
    DarkMagenta=0x8B008B,
    DarkOliveGreen=0x556B2F,
    DarkOrange=0xFF8C00,
    DarkOrchid=0x9932CC,
    DarkRed=0x8B0000,
    DarkSalmon=0xE9967A,
    DarkSeaGreen=0x8FBC8F,
    DarkSlateBlue=0x483D8B,
    DarkSlateGray=0x2F4F4F,
    DarkSlateGrey=0x2F4F4F,
    DarkTurquoise=0x00CED1,
    DarkViolet=0x9400D3,
    DeepPink=0xFF1493,
    DeepSkyBlue=0x00BFFF,
    DimGray=0x696969,
    DimGrey=0x696969,
    DodgerBlue=0x1E90FF,
    FireBrick=0xB22222,
    FloralWhite=0xFFFAF0,
    ForestGreen=0x228B22,
    Fuchsia=0xFF00FF,
    Gainsboro=0xDCDCDC,
    GhostWhite=0xF8F8FF,
    Gold=0xFFD700,
    Goldenrod=0xDAA520,
    Gray=0x808080,
    Grey=0x808080,
    Green=0x008000,
    GreenYellow=0xADFF2F,
    Honeydew=0xF0FFF0,
    HotPink=0xFF69B4,
    IndianRed=0xCD5C5C,
    Indigo=0x4B0082,
    Ivory=0xFFFFF0,
    Khaki=0xF0E68C,
    Lavender=0xE6E6FA,
    LavenderBlush=0xFFF0F5,
    LawnGreen=0x7CFC00,
    LemonChiffon=0xFFFACD,
    LightBlue=0xADD8E6,
    LightCoral=0xF08080,
    LightCyan=0xE0FFFF,
    LightGoldenrodYellow=0xFAFAD2,
    LightGreen=0x90EE90,
    LightGrey=0xD3D3D3,
    LightPink=0xFFB6C1,
    LightSalmon=0xFFA07A,
    LightSeaGreen=0x20B2AA,
    LightSkyBlue=0x87CEFA,
    LightSlateGray=0x778899,
    LightSlateGrey=0x778899,
    LightSteelBlue=0xB0C4DE,
    LightYellow=0xFFFFE0,
    Lime=0x00FF00,
    LimeGreen=0x32CD32,
    Linen=0xFAF0E6,
    Magenta=0xFF00FF,
    Maroon=0x800000,
    MediumAquamarine=0x66CDAA,
    MediumBlue=0x0000CD,
    MediumOrchid=0xBA55D3,
    MediumPurple=0x9370DB,
    MediumSeaGreen=0x3CB371,
    MediumSlateBlue=0x7B68EE,
    MediumSpringGreen=0x00FA9A,
    MediumTurquoise=0x48D1CC,
    MediumVioletRed=0xC71585,
    MidnightBlue=0x191970,
    MintCream=0xF5FFFA,
    MistyRose=0xFFE4E1,
    Moccasin=0xFFE4B5,
    NavajoWhite=0xFFDEAD,
    Navy=0x000080,
    OldLace=0xFDF5E6,
    Olive=0x808000,
    OliveDrab=0x6B8E23,
    Orange=0xFFA500,
    OrangeRed=0xFF4500,
    Orchid=0xDA70D6,
    PaleGoldenrod=0xEEE8AA,
    PaleGreen=0x98FB98,
    PaleTurquoise=0xAFEEEE,
    PaleVioletRed=0xDB7093,
    PapayaWhip=0xFFEFD5,
    PeachPuff=0xFFDAB9,
    Peru=0xCD853F,
    Pink=0xFFC0CB,
    Plaid=0xCC5533,
    Plum=0xDDA0DD,
    PowderBlue=0xB0E0E6,
    Purple=0x800080,
    Red=0xFF0000,
    RosyBrown=0xBC8F8F,
    RoyalBlue=0x4169E1,
    SaddleBrown=0x8B4513,
    Salmon=0xFA8072,
    SandyBrown=0xF4A460,
    SeaGreen=0x2E8B57,
    Seashell=0xFFF5EE,
    Sienna=0xA0522D,
    Silver=0xC0C0C0,
    SkyBlue=0x87CEEB,
    SlateBlue=0x6A5ACD,
    SlateGray=0x708090,
    SlateGrey=0x708090,
    Snow=0xFFFAFA,
    SpringGreen=0x00FF7F,
    SteelBlue=0x4682B4,
    Tan=0xD2B48C,
    Teal=0x008080,
    Thistle=0xD8BFD8,
    Tomato=0xFF6347,
    Turquoise=0x40E0D0,
    Violet=0xEE82EE,
    Wheat=0xF5DEB3,
    White=0xFFFFFF,
    WhiteSmoke=0xF5F5F5,
    Yellow=0xFFFF00,
    YellowGreen=0x9ACD32,

    // LED RGB color that roughly approximates
    // the color of incandescent fairy lights,
    // assuming that you're using FastLED
    // color correction on your LEDs (recommended).
    FairyLight=0xFFE42D,
    // If you are using no color correction, use this
    FairyLightNCC=0xFF9D2A
  };
};

extern const TProgmemRGBPalette16 CloudColors_p FL_PROGMEM =
{
    CRGB::Blue,
    CRGB::DarkBlue,
    CRGB::DarkBlue,
    CRGB::DarkBlue,

    CRGB::DarkBlue,
    CRGB::DarkBlue,
    CRGB::DarkBlue,
    CRGB::DarkBlue,

    CRGB::Blue,
    CRGB::DarkBlue,
    CRGB::SkyBlue,
    CRGB::SkyBlue,

    CRGB::LightBlue,
    CRGB::White,
    CRGB::LightBlue,
    CRGB::SkyBlue
};

extern const TProgmemRGBPalette16 LavaColors_p FL_PROGMEM =
{
    CRGB::Black,
    CRGB::Maroon,
    CRGB::Black,
    CRGB::Maroon,

    CRGB::DarkRed,
    CRGB::Maroon,
    CRGB::DarkRed,

    CRGB::DarkRed,
    CRGB::DarkRed,
    CRGB::Red,
    CRGB::Orange,

    CRGB::White,
    CRGB::Orange,
    CRGB::Red,
    CRGB::DarkRed
};


extern const TProgmemRGBPalette16 OceanColors_p FL_PROGMEM =
{
    CRGB::MidnightBlue,
    CRGB::DarkBlue,
    CRGB::MidnightBlue,
    CRGB::Navy,

    CRGB::DarkBlue,
    CRGB::MediumBlue,
    CRGB::SeaGreen,
    CRGB::Teal,

    CRGB::CadetBlue,
    CRGB::Blue,
    CRGB::DarkCyan,
    CRGB::CornflowerBlue,

    CRGB::Aquamarine,
    CRGB::SeaGreen,
    CRGB::Aqua,
    CRGB::LightSkyBlue
};

extern const TProgmemRGBPalette16 ForestColors_p FL_PROGMEM =
{
    CRGB::DarkGreen,
    CRGB::DarkGreen,
    CRGB::DarkOliveGreen,
    CRGB::DarkGreen,

    CRGB::Green,
    CRGB::ForestGreen,
    CRGB::OliveDrab,
    CRGB::Green,

    CRGB::SeaGreen,
    CRGB::MediumAquamarine,
    CRGB::LimeGreen,
    CRGB::YellowGreen,

    CRGB::LightGreen,
    CRGB::LawnGreen,
    CRGB::MediumAquamarine,
    CRGB::ForestGreen
};

/// HSV Rainbow
extern const TProgmemRGBPalette16 RainbowColors_p FL_PROGMEM =
{
    0xFF0000, 0xD52A00, 0xAB5500, 0xAB7F00,
    0xABAB00, 0x56D500, 0x00FF00, 0x00D52A,
    0x00AB55, 0x0056AA, 0x0000FF, 0x2A00D5,
    0x5500AB, 0x7F0081, 0xAB0055, 0xD5002B
};

/// HSV Rainbow colors with alternatating stripes of black
#define RainbowStripesColors_p RainbowStripeColors_p
extern const TProgmemRGBPalette16 RainbowStripeColors_p FL_PROGMEM =
{
    0xFF0000, 0x000000, 0xAB5500, 0x000000,
    0xABAB00, 0x000000, 0x00FF00, 0x000000,
    0x00AB55, 0x000000, 0x0000FF, 0x000000,
    0x5500AB, 0x000000, 0xAB0055, 0x000000
};

/// HSV color ramp: blue purple ping red orange yellow (and back)
/// Basically, everything but the greens, which tend to make
/// people's skin look unhealthy.  This palette is good for
/// lighting at a club or party, where it'll be shining on people.
extern const TProgmemRGBPalette16 PartyColors_p FL_PROGMEM =
{
    0x5500AB, 0x84007C, 0xB5004B, 0xE5001B,
    0xE81700, 0xB84700, 0xAB7700, 0xABAB00,
    0xAB5500, 0xDD2200, 0xF2000E, 0xC2003E,
    0x8F0071, 0x5F00A1, 0x2F00D0, 0x0007F9
};

/// Approximate "black body radiation" palette, akin to
/// the FastLED 'HeatColor' function.
/// Recommend that you use values 0-240 rather than
/// the usual 0-255, as the last 15 colors will be
/// 'wrapping around' from the hot end to the cold end,
/// which looks wrong.
extern const TProgmemRGBPalette16 HeatColors_p FL_PROGMEM =
{
    0x000000,
    0x330000, 0x660000, 0x990000, 0xCC0000, 0xFF0000,
    0xFF3300, 0xFF6600, 0xFF9900, 0xFFCC00, 0xFFFF00,
    0xFFFF33, 0xFFFF66, 0xFFFF99, 0xFFFFCC, 0xFFFFFF
};

static inline RgbaColor flToDf(CRGB ori) {
  return RgbaColor(ori.r, ori.g, ori.b);
}

enum OurColorPalette {
  OCPCloud = 0,
  OCPLava = 1,
  OCPOcean = 2,
  OCPForest = 3,
  OCPRainbow = 4,
  OCPParty = 5,
  OCPHeat = 6,
};

static inline RgbaColor colorFromOurPalette(OurColorPalette ocp, uint8_t color) {
#define RET_COLOR(c) return flToDf(c##Colors_p[color >> 4])
#define CASE_RET_COLOR(c) case OCP##c: RET_COLOR(c)
  switch (ocp) {
    CASE_RET_COLOR(Cloud);
    CASE_RET_COLOR(Lava);
    CASE_RET_COLOR(Ocean);
    CASE_RET_COLOR(Forest);
    CASE_RET_COLOR(Rainbow);
    CASE_RET_COLOR(Party);
    CASE_RET_COLOR(Heat);
  }
  RET_COLOR(Rainbow);
#undef CASE_RET_COLOR
#undef RET_COLOR
}

class SpinPlasma : public Effect {
  Color color(const Pixel& pixel) const override {
    using internal::sin8;
    using internal::cos8;

    constexpr int32_t speed = PLASMA_SPEED;
    uint8_t offset = speed * pixel.frame.time / 255;
    int plasVector = offset;

    // Calculate current center of plasma pattern (can be offscreen)
    int xOffset = (cos8(plasVector)-127)/2;
    int yOffset = (sin8(plasVector)-127)/2;

    uint8_t color = sin8(sqrt(square(((float)pixel.coord.x - 7.5) * 12 + xOffset) +
                              square(((float)pixel.coord.y - 2) * 12 + yOffset)) + offset);
    return colorFromOurPalette(ocp_, color);
  }

  size_t contextSize(const Animation&) const override { return 0; }
  void begin(const Frame&) const override {}
  void rewind(const Frame& /*frame*/) const override {}

  OurColorPalette ocp_ = OCPRainbow;
public:
  SpinPlasma(OurColorPalette ocp) : ocp_(ocp) {}
};

auto spinplasma = []() {
  return effect([](const Frame & frame) {
    using internal::sin8;
    using internal::cos8;

    OurColorPalette ocp_ = OCPRainbow;

    constexpr int32_t speed = PLASMA_SPEED;
    uint8_t offset = speed * frame.time / 255;
    int plasVector = offset;

    // Calculate current center of plasma pattern (can be offscreen)
    int xOffset = (cos8(plasVector)-127)/2;
    int yOffset = (sin8(plasVector)-127)/2;

    return [ = ](const Pixel& px) -> Color {
      uint8_t color = sin8(sqrt(square(((float)px.coord.x - 7.5) * 12 + xOffset) +
                                square(((float)px.coord.y - 2) * 12 + yOffset)) + offset);
      return colorFromOurPalette(ocp_, color);
    };
  });
};

} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_PLASMA_H */
