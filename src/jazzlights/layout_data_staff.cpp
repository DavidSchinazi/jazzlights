#include "jazzlights/layout_data.h"

#if JL_IS_CONFIG(STAFF)

#include "jazzlights/layouts/pixelmap.h"

namespace jazzlights {
namespace {

constexpr Point pixelMap[] = {
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
    {2.00, 15.00},
    {2.00, 16.00},
    {2.00, 17.00},
    {2.00, 18.00},
    {2.00, 19.00},
    {2.00, 20.00},
    {2.00, 21.00},
    {2.00, 22.00},
    {2.00, 23.00},
    {2.00, 24.00},
    {2.00, 25.00},
    {2.00, 26.00},
    {2.00, 27.00},
    {2.00, 28.00},
    {2.00, 29.00},
    {2.00, 30.00},
    {2.00, 31.00},
    {2.00, 32.00},
    {2.00, 33.00},
    {2.00, 34.00},
    {2.00, 35.00},
    {2.00, 36.00},
};

static_assert(JL_LENGTH(pixelMap) == 36, "bad size");
PixelMap pixels(JL_LENGTH(pixelMap), pixelMap);

constexpr Point pixelMap2[] = {
    {0.00,  0.00},
    {1.00,  0.00},
    {2.00,  0.00},
    {3.00,  0.00},
    {4.00,  0.00},
    {3.00,  0.00},
    {2.00,  0.00},
    {1.00,  0.00},
    {0.00, -1.00},
    {1.00, -1.00},
    {2.00, -1.00},
    {3.00, -1.00},
    {4.00, -1.00},
    {3.00, -1.00},
    {2.00, -1.00},
    {1.00, -1.00},
    {0.00, -2.00},
    {1.00, -2.00},
    {2.00, -2.00},
    {3.00, -2.00},
    {4.00, -2.00},
    {3.00, -2.00},
    {2.00, -2.00},
    {1.00, -2.00},
    {0.00, -3.00},
    {1.00, -3.00},
    {2.00, -3.00},
    {3.00, -3.00},
    {4.00, -3.00},
    {3.00, -3.00},
    {2.00, -3.00},
    {1.00, -3.00},
    {0.00, -4.00},
};

static_assert(JL_LENGTH(pixelMap2) == 33, "bad size");
PixelMap pixels2(JL_LENGTH(pixelMap2), pixelMap2);

}  // namespace

void AddLedsToRunner(FastLedRunner* runner) {
  runner->AddLeds<WS2811, LED_PIN, RGB>(pixels);
  runner->AddLeds<WS2812B, LED_PIN2, GRB>(pixels2);
}

}  // namespace jazzlights

#endif  // STAFF
