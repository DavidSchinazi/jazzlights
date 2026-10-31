#include "jazzlights/layout/layout_data_clouds.h"

#if JL_IS_CONFIG(CLOUDS)

#include "jazzlights/layout/matrix.h"
#include "jazzlights/layout/pixelmap.h"

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
PixelMap sCloudPixels(JL_LENGTH(cloudPixelMap), cloudPixelMap);

// The ceiling strands flicker from their 18th LED onwards, even at low brightness, so we only send data to their first
// 17 LEDs for now. The LEDs after that keep whatever they last displayed until they lose power.
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
};

static_assert(JL_LENGTH(ceiling1PixelMap) == 17, "bad size");
PixelMap sCeiling1Pixels(JL_LENGTH(ceiling1PixelMap), ceiling1PixelMap);

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
};

static_assert(JL_LENGTH(ceiling2PixelMap) == 17, "bad size");
PixelMap sCeiling2Pixels(JL_LENGTH(ceiling2PixelMap), ceiling2PixelMap);

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
    {3.00, -11.00},
    {3.00, -12.00},
    {3.00, -13.00},
    {3.00, -14.00},
    {3.00, -15.00},
    {3.00, -16.00},
    {3.00, -17.00},
};

static_assert(JL_LENGTH(ceiling3PixelMap) == 17, "bad size");
PixelMap sCeiling3Pixels(JL_LENGTH(ceiling3PixelMap), ceiling3PixelMap);

}  // namespace

Layout* GetCloudsLayout() { return &sCloudPixels; }

void AddLedsToRunner(FastLedRunner* runner) {
#if JL_IS_CONTROLLER(ATOM_MATRIX) || JL_IS_CONTROLLER(ATOM_LITE)
  constexpr uint8_t kCloudsPin = 21;
  constexpr uint8_t kCeiling1Pin = 25;
  constexpr uint8_t kCeiling2Pin = 33;
  constexpr uint8_t kCeiling3Pin = 23;
#elif JL_IS_CONTROLLER(ATOM_S3) || JL_IS_CONTROLLER(ATOM_S3_LITE)
  constexpr uint8_t kCloudsPin = 39;
  constexpr uint8_t kCeiling1Pin = 38;
  constexpr uint8_t kCeiling2Pin = 8;
  constexpr uint8_t kCeiling3Pin = 7;
#else
#error "Clouds config does not currently support this controller"
#endif
  // Clouds.
  runner->AddLeds<WS2812B, kCloudsPin, GRB>(sCloudPixels);
  // Ceiling.
  runner->AddLeds<WS2812B, kCeiling1Pin, GRB>(sCeiling1Pixels);
  runner->AddLeds<WS2812B, kCeiling2Pin, GRB>(sCeiling2Pixels);
  runner->AddLeds<WS2812B, kCeiling3Pin, GRB>(sCeiling3Pixels);
}

}  // namespace jazzlights

#endif  // CLOUDS
