#include "jazzlights/layouts/layout_data.h"

#if JL_IS_CONFIG(GAUNTLET) || JL_IS_CONFIG(HAMMER) || JL_IS_CONFIG(FAIRY_WAND) || JL_IS_CONFIG(ROPELIGHT) || \
    JL_IS_CONFIG(SHOE) || JL_IS_CONFIG(XMAS_TREE)

#include "jazzlights/layouts/matrix.h"

namespace jazzlights {
namespace {

#if JL_IS_CONFIG(GAUNTLET)
Matrix pixels(/*w=*/20, /*h=*/15);
#endif  // GAUNTLET

#if JL_IS_CONFIG(HAMMER)
Matrix pixels(/*w=*/20, /*h=*/1);
#endif  // HAMMER

#if JL_IS_CONFIG(FAIRY_WAND)
Matrix pixels(/*w=*/3, /*h=*/3, /*resolution=*/1.0);
#endif  // FAIRY_WAND

#if JL_IS_CONFIG(ROPELIGHT)
Matrix pixels(/*w=*/300, /*h=*/1);
#endif  // ROPELIGHT

#if JL_IS_CONFIG(SHOE)
Matrix pixels(/*w=*/36, /*h=*/1);
#endif  // ROPELIGHT

#if JL_IS_CONFIG(XMAS_TREE)
Matrix pixels(/*w=*/10, /*h=*/10);
#endif  // XMAS_TREE

}  // namespace

void AddLedsToRunner(FastLedRunner* runner) {
#if JL_IS_CONFIG(ROPELIGHT)
  runner->AddLeds<WS2812, LED_PIN, BRG>(pixels);
#elif JL_IS_CONFIG(XMAS_TREE)
  runner->AddLeds<WS2812B, LED_PIN, RGB>(pixels);
#else
  runner->AddLeds<WS2812B, LED_PIN, GRB>(pixels);
#endif
}

}  // namespace jazzlights

#elif JL_IS_CONFIG(PHONE)
namespace jazzlights {
void AddLedsToRunner(FastLedRunner* runner) {}
}  // namespace jazzlights
#endif
