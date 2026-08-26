#include "jazzlights/layout/layout_data.h"

#if JL_IS_CONFIG(STAFF)

#include "jazzlights/layout/pixelmap.h"

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
PixelMap sPixels(JL_LENGTH(pixelMap), pixelMap);

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
PixelMap sPixels2(JL_LENGTH(pixelMap2), pixelMap2);

}  // namespace

void AddLedsToRunner(FastLedRunner* runner) {
  // The LEDs in the shaft of the staff are physically 24V WS2811 pixels, but we deliberately clock them with WS2812B
  // timings. FastLED 3.7.6 and earlier routed ESP-IDF 5 through the legacy RMT4 driver, which drove the pins off the
  // 80MHz APB clock and emitted WS2811's intended 320/960/640/640ns pulses. FastLED 3.9.0 switched to the RMT5 backend,
  // whose encoder runs at 10MHz and truncates every pulse onto a 100ns grid, so those become 300/900/600/600ns and
  // leave only 300ns separating a zero-bit high from a one-bit high. That was too little margin for this strip and it
  // stayed completely dark on every version from 3.9.5 on. WS2812B timings quantize to 200/1000/800/300ns, which
  // doubles the margin to 600ns and these WS2811 driver ICs decode it fine.
  runner->AddLeds<WS2812B, LED_PIN, RGB>(sPixels);
  // The LEDs in the battery compartment at the top of the staff are regular WS2812B pixels.
  runner->AddLeds<WS2812B, LED_PIN2, GRB>(sPixels2);
}

}  // namespace jazzlights

#endif  // STAFF
