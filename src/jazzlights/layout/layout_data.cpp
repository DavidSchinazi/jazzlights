#include "jazzlights/layout/layout_data.h"

#if JL_IS_CONFIG(GAUNTLET) || JL_IS_CONFIG(HAMMER) || JL_IS_CONFIG(FAIRY_WAND) || JL_IS_CONFIG(ROPELIGHT) || \
    JL_IS_CONFIG(SHOE) || JL_IS_CONFIG(XMAS_TREE) || JL_IS_CONFIG(CREATURE) || JL_IS_CONFIG(FAIRY_STRING) || \
    JL_IS_CONFIG(ORRERY_PLANET)

#include "jazzlights/layout/matrix.h"

namespace jazzlights {
namespace {

#if JL_AUDIO_VISUALIZER
Matrix pixels(/*w=*/50, /*h=*/1);
#elif JL_IS_CONFIG(GAUNTLET)
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
#endif  // SHOE

#if JL_IS_CONFIG(XMAS_TREE)
Matrix pixels(/*w=*/10, /*h=*/10);
#endif  // XMAS_TREE

#if JL_IS_CONFIG(FAIRY_STRING)
Matrix pixels(/*w=*/20, /*h=*/20);
#endif  // FAIRY_STRING

#if JL_IS_CONFIG(CREATURE)
Matrix pixels(/*w=*/160, /*h=*/1);
#endif  // CREATURE

#if JL_IS_CONFIG(ORRERY_PLANET)
Matrix pixels(/*w=*/35, /*h=*/1);
#endif  // ORRERY_PLANET

}  // namespace

void AddLedsToRunner(FastLedRunner* runner) {
#if JL_AUDIO_VISUALIZER
  runner->AddLeds<WS2812B, kPinA2, RGB>(pixels);
  runner->AddLeds<WS2812B, kPinA1, RGB>(pixels);
  runner->AddLeds<WS2812B, kPinB2, RGB>(pixels);
#elif JL_IS_CONFIG(ROPELIGHT)
  runner->AddLeds<WS2812, LED_PIN, BRG>(pixels);
#elif JL_IS_CONFIG(XMAS_TREE) || JL_IS_CONFIG(FAIRY_STRING)
  runner->AddLeds<WS2812B, LED_PIN, RGB>(pixels);
#else
  runner->AddLeds<WS2812B, LED_PIN, GRB>(pixels);
#endif
}

}  // namespace jazzlights

#elif JL_IS_CONFIG(PHONE) || JL_IS_CONFIG(ORRERY_LEADER)
namespace jazzlights {
void AddLedsToRunner(FastLedRunner* runner) {}
}  // namespace jazzlights
#endif
