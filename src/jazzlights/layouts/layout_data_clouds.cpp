#include "jazzlights/layouts/layout_data_clouds.h"

#if JL_IS_CONFIG(CLOUDS)

#include "jazzlights/layouts/matrix.h"
#include "jazzlights/layouts/pixelmap.h"

namespace jazzlights {
namespace {

constexpr Point cloudPixelMap[] = {
    {1.00,  0.00},
    {1.00,  1.00},
    {1.00,  2.00},
    {1.00,  3.00},
    {1.00,  4.00},
    {1.00,  5.00},
    {1.00,  6.00},
    {1.00,  7.00},
    {1.00,  8.00},
    {1.00,  9.00},
    {1.00, 10.00},
    {1.00, 11.00},
    {1.00, 12.00},
    {1.00, 13.00},
    {1.00, 14.00},
    {1.00, 15.00},
    {1.00, 16.00},
    EmptyPoint(),  EmptyPoint(),  {2.00,  0.00},
    {2.00,  1.00},
    {2.00,  2.00},
    {2.00,  3.00},
    {2.00,  4.00},
    {2.00,  5.00},
    {2.00,  6.00},
    {2.00,  7.00},
    {2.00,  8.00},
    {2.00,  9.00},
    {2.00, 10.00},
    {2.00, 11.00},
    {2.00, 12.00},
    {2.00, 13.00},
    {2.00, 14.00},
    EmptyPoint(),
    EmptyPoint(),  EmptyPoint(),  {3.00,  0.00},
    {3.00,  1.00},
    {3.00,  2.00},
    {3.00,  3.00},
    {3.00,  4.00},
    {3.00,  5.00},
    {3.00,  6.00},
    {3.00,  7.00},
    {3.00,  8.00},
    {3.00,  9.00},
    {3.00, 10.00},
    {3.00, 11.00},
    {3.00, 12.00},
    EmptyPoint(),  EmptyPoint(),  EmptyPoint(),  {4.00,  0.00},
    {4.00,  1.00},
    {4.00,  2.00},
    {4.00,  3.00},
    {4.00,  4.00},
    {4.00,  5.00},
    {4.00,  6.00},
    {4.00,  7.00},
    {4.00,  8.00},
    {4.00,  9.00},
    {4.00, 10.00},
    EmptyPoint(),  EmptyPoint(),  {5.00,  0.00},
    {5.00,  1.00},
    {5.00,  2.00},
    {5.00,  3.00},
    {5.00,  4.00},
    {5.00,  5.00},
    {5.00,  6.00},
    {5.00,  7.00},
    {5.00,  8.00},
    {5.00,  9.00},
    {5.00, 10.00},
    {5.00, 11.00},
    {5.00, 12.00},
    {5.00, 13.00},
    EmptyPoint(),  EmptyPoint(),  {6.00,  0.00},
    {6.00,  1.00},
    {6.00,  2.00},
    {6.00,  3.00},
    {6.00,  4.00},
    {6.00,  5.00},
    {6.00,  6.00},
    EmptyPoint(),  {7.00,  0.00},
    {7.00,  1.00},
    {7.00,  2.00},
    {7.00,  3.00},
    {7.00,  4.00},
    {7.00,  5.00},
    {7.00,  6.00},
    {7.00,  7.00},
    {7.00,  8.00},
    {7.00,  9.00},
};

static_assert(JL_LENGTH(cloudPixelMap) == 100, "bad size");
PixelMap cloudPixels(JL_LENGTH(cloudPixelMap), cloudPixelMap);

constexpr Point ceiling1PixelMap[] = {
    {1.00,  -1.00},
    {1.00,  -2.00},
    {1.00,  -3.00},
    {1.00,  -4.00},
    {1.00,  -5.00},
    {1.00,  -6.00},
    {1.00,  -7.00},
    {1.00,  -8.00},
    {1.00,  -9.00},
    {1.00, -10.00},
    {1.00, -11.00},
    {1.00, -12.00},
    {1.00, -13.00},
    {1.00, -14.00},
    {1.00, -15.00},
    {1.00, -16.00},
    {1.00, -17.00},
    {1.00, -18.00},
    {1.00, -19.00},
    {1.00, -20.00},
    {1.00, -21.00},
    {1.00, -22.00},
    {1.00, -23.00},
    {1.00, -24.00},
    {1.00, -25.00},
    {1.00, -26.00},
    {1.00, -27.00},
    {1.00, -28.00},
    {1.00, -29.00},
    {1.00, -30.00},
    {1.00, -31.00},
    {1.00, -32.00},
    {1.00, -33.00},
    {1.00, -34.00},
    {1.00, -35.00},
    {1.00, -36.00},
    {1.00, -37.00},
    {1.00, -38.00},
    {1.00, -39.00},
    {1.00, -40.00},
    {1.00, -41.00},
    {1.00, -42.00},
    EmptyPoint(),   EmptyPoint(),   EmptyPoint(),   EmptyPoint(),
};

static_assert(JL_LENGTH(ceiling1PixelMap) == 46, "bad size");
PixelMap ceiling1Pixels(JL_LENGTH(ceiling1PixelMap), ceiling1PixelMap);

constexpr Point ceiling2PixelMap[] = {
    {2.00,  -1.00},
    {2.00,  -2.00},
    {2.00,  -3.00},
    {2.00,  -4.00},
    {2.00,  -5.00},
    {2.00,  -6.00},
    {2.00,  -7.00},
    {2.00,  -8.00},
    {2.00,  -9.00},
    {2.00, -10.00},
    {2.00, -11.00},
    {2.00, -12.00},
    {2.00, -13.00},
    {2.00, -14.00},
    {2.00, -15.00},
    {2.00, -16.00},
    {2.00, -17.00},
    {2.00, -18.00},
    {2.00, -19.00},
    {2.00, -20.00},
    {2.00, -21.00},
    {2.00, -22.00},
    {2.00, -23.00},
    {2.00, -24.00},
    {2.00, -25.00},
    {2.00, -26.00},
    {2.00, -27.00},
    {2.00, -28.00},
    {2.00, -29.00},
    {2.00, -30.00},
    {2.00, -31.00},
    {2.00, -32.00},
    {2.00, -33.00},
    {2.00, -34.00},
    {2.00, -35.00},
    {2.00, -36.00},
    {2.00, -37.00},
    {2.00, -38.00},
    EmptyPoint(),   EmptyPoint(),   EmptyPoint(),   EmptyPoint(),
};

static_assert(JL_LENGTH(ceiling2PixelMap) == 42, "bad size");
PixelMap ceiling2Pixels(JL_LENGTH(ceiling2PixelMap), ceiling2PixelMap);

constexpr Point ceiling3PixelMap[] = {
    {3.00,  -1.00},
    {3.00,  -2.00},
    {3.00,  -3.00},
    {3.00,  -4.00},
    {3.00,  -5.00},
    {3.00,  -6.00},
    {3.00,  -7.00},
    {3.00,  -8.00},
    {3.00,  -9.00},
    {3.00, -10.00},
    EmptyPoint(),  EmptyPoint(),  EmptyPoint(),  EmptyPoint(),
    EmptyPoint(),  EmptyPoint(),  EmptyPoint(),   EmptyPoint(),  EmptyPoint(),  EmptyPoint(),  EmptyPoint(),
    EmptyPoint(),  EmptyPoint(),  EmptyPoint(),   EmptyPoint(),  EmptyPoint(),  EmptyPoint(),  EmptyPoint(),
    EmptyPoint(),  EmptyPoint(),  EmptyPoint(),   EmptyPoint(),
};

static_assert(JL_LENGTH(ceiling3PixelMap) == 32, "bad size");
PixelMap ceiling3Pixels(JL_LENGTH(ceiling3PixelMap), ceiling3PixelMap);

}  // namespace

Layout* GetCloudsLayout() { return &cloudPixels; }

void AddLedsToRunner(FastLedRunner* runner) {
#if JL_IS_CONTROLLER(ATOM_MATRIX)
  constexpr uint8_t kCloudsPin = 21;
  constexpr uint8_t kCeiling1Pin = 25;
  constexpr uint8_t kCeiling2Pin = 33;
  constexpr uint8_t kCeiling3Pin = 23;
#elif JL_IS_CONTROLLER(ATOM_S3)
  constexpr uint8_t kCloudsPin = 39;
  constexpr uint8_t kCeiling1Pin = 38;
  constexpr uint8_t kCeiling2Pin = 8;
  constexpr uint8_t kCeiling3Pin = 7;
#else
#error "Clouds config does not currently support this controller"
#endif
  // Clouds.
  runner->AddLeds<WS2812B, kCloudsPin, GRB>(cloudPixels);
  // Ceiling.
  runner->AddLeds<WS2812B, kCeiling1Pin, GRB>(ceiling1Pixels);
  runner->AddLeds<WS2812B, kCeiling2Pin, GRB>(ceiling2Pixels);
  runner->AddLeds<WS2812B, kCeiling3Pin, GRB>(ceiling3Pixels);
}

}  // namespace jazzlights

#endif  // CLOUDS
