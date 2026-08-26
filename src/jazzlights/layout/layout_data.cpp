#include "jazzlights/layout/layout_data.h"

#if JL_IS_CONFIG(GAUNTLET) || JL_IS_CONFIG(HAMMER) || JL_IS_CONFIG(FAIRY_WAND) || JL_IS_CONFIG(ROPELIGHT) || \
    JL_IS_CONFIG(SHOE) || JL_IS_CONFIG(XMAS_TREE) || JL_IS_CONFIG(CREATURE) || JL_IS_CONFIG(FAIRY_STRING) || \
    JL_IS_CONFIG(ORRERY_PLANET) || JL_IS_CONFIG(NEW_HAT) || JL_IS_CONFIG(BOW) || JL_IS_CONFIG(RHINO_HAT) ||  \
    JL_IS_CONFIG(RHINO_STAFF)

#include "jazzlights/layout/matrix.h"
#include "jazzlights/layout/pixelmap.h"

namespace jazzlights {
namespace {

#if JL_AUDIO_VISUALIZER
Matrix sPixels(/*w=*/50, /*h=*/1);
#elif JL_IS_CONFIG(GAUNTLET)
Matrix sPixels(/*w=*/20, /*h=*/15);
#endif  // GAUNTLET

#if JL_IS_CONFIG(HAMMER)
Matrix sPixels(/*w=*/20, /*h=*/1);
#endif  // HAMMER

#if JL_IS_CONFIG(FAIRY_WAND)
Matrix sPixels(/*w=*/3, /*h=*/3, /*resolution=*/1.0);
#endif  // FAIRY_WAND

#if JL_IS_CONFIG(ROPELIGHT)
Matrix sPixels(/*w=*/300, /*h=*/1);
#endif  // ROPELIGHT

#if JL_IS_CONFIG(SHOE)
Matrix sPixels(/*w=*/36, /*h=*/1);
#endif  // SHOE

#if JL_IS_CONFIG(XMAS_TREE)
Matrix sPixels(/*w=*/10, /*h=*/10);
#endif  // XMAS_TREE

#if JL_IS_CONFIG(FAIRY_STRING)
Matrix sPixels(/*w=*/20, /*h=*/20);
#endif  // FAIRY_STRING

#if JL_IS_CONFIG(CREATURE)
Matrix sPixels(/*w=*/160, /*h=*/1);
#endif  // CREATURE

#if JL_IS_CONFIG(ORRERY_PLANET)
Matrix sPixels(/*w=*/35, /*h=*/1);
#endif  // ORRERY_PLANET

#if JL_IS_CONFIG(NEW_HAT)
constexpr Point pixelMap[] = {
    { 0.00, 0.00},
    { 1.00, 0.00},
    { 2.00, 0.00},
    { 3.00, 0.00},
    { 4.00, 0.00},
    { 5.00, 0.00},
    { 6.00, 0.00},
    { 7.00, 0.00},
    { 8.00, 0.00},
    { 9.00, 0.00},
    {10.00, 0.00},
    {11.00, 0.00},
    {12.00, 0.00},
    {13.00, 0.00},
    {14.00, 0.00},
    {15.00, 0.00},
    {16.00, 0.00},
    {17.00, 0.00},
    {18.00, 0.00},
    {19.00, 0.00},
    {20.00, 0.00},
    {21.00, 0.00},
    {22.00, 0.00},
    {23.00, 0.00},
    {24.00, 0.00},
    {25.00, 0.00},
    {26.00, 0.00},
    {27.00, 0.00},
    {28.00, 0.00},
    {29.00, 0.00},
    {30.00, 0.00},
    {31.00, 0.00},
    {32.00, 0.00},
    {33.00, 0.00},
    {34.00, 0.00},
    {35.00, 0.00},
    {36.00, 0.00},
    {37.00, 0.00},
    {38.00, 0.00},
    {39.00, 0.00},
    {40.00, 0.00},
    {41.00, 0.00},
    {42.00, 0.00},
    {43.00, 0.00},
    {44.00, 0.00},
    {45.00, 0.00},
    {46.00, 0.00},
    {47.00, 0.00},
    {48.00, 0.00},
    {47.00, 0.00},
    {46.00, 0.00},
    {45.00, 0.00},
    {44.00, 0.00},
    {43.00, 0.00},
    {42.00, 0.00},
    {41.00, 0.00},
    {40.00, 0.00},
    {39.00, 0.00},
    {38.00, 0.00},
    {37.00, 0.00},
    {36.00, 0.00},
    {35.00, 0.00},
    {34.00, 0.00},
    {33.00, 0.00},
    {32.00, 0.00},
    {31.00, 0.00},
    {30.00, 0.00},
    {29.00, 0.00},
    {28.00, 0.00},
    {27.00, 0.00},
    {26.00, 0.00},
    {25.00, 0.00},
    {24.00, 0.00},
    {23.00, 0.00},
    {22.00, 0.00},
    {21.00, 0.00},
    {20.00, 0.00},
    {19.00, 0.00},
    {18.00, 0.00},
    {17.00, 0.00},
    {16.00, 0.00},
    {15.00, 0.00},
    {14.00, 0.00},
    {13.00, 0.00},
    {12.00, 0.00},
    {11.00, 0.00},
    {10.00, 0.00},
    { 9.00, 0.00},
    { 8.00, 0.00},
    { 7.00, 0.00},
    { 6.00, 0.00},
    { 5.00, 0.00},
    { 4.00, 0.00},
    { 3.00, 0.00},
    { 2.00, 0.00},
    { 1.00, 0.00},
    { 0.00, 0.00},
    { 1.00, 0.00},
};

static_assert(JL_LENGTH(pixelMap) == 98, "bad size");
PixelMap sPixels(JL_LENGTH(pixelMap), pixelMap);
#endif  // NEW_HAT

#if JL_IS_CONFIG(RHINO_HAT)
Matrix sPixels(/*w=*/50, /*h=*/1);
#endif  // RHINO_HAT

#if JL_IS_CONFIG(RHINO_STAFF)
Matrix sPixels(/*w=*/480, /*h=*/1);
#endif  // RHINO_STAFF

#if JL_IS_CONFIG(BOW)
Matrix sPixels(/*w=*/40, /*h=*/1);
#endif  // BOW

}  // namespace

void AddLedsToRunner(FastLedRunner* runner) {
#if JL_AUDIO_VISUALIZER && !(JL_IS_CONTROLLER(M5STICK_C))
  runner->AddLeds<WS2812B, kPinA2, RGB>(sPixels);
  runner->AddLeds<WS2812B, kPinA1, RGB>(sPixels);
  runner->AddLeds<WS2812B, kPinB2, RGB>(sPixels);
#elif JL_IS_CONFIG(ROPELIGHT)
  runner->AddLeds<WS2812B, LED_PIN, BRG>(sPixels);
#elif JL_IS_CONFIG(NEW_HAT)
  runner->AddLeds<WS2812B, 2, GRB>(sPixels);
#elif JL_IS_CONFIG(BOW)
  runner->AddLeds<WS2812B, 2, RGB>(sPixels);
#elif JL_IS_CONFIG(XMAS_TREE) || JL_IS_CONFIG(FAIRY_STRING)
  runner->AddLeds<WS2812B, LED_PIN, RGB>(sPixels);
#else
  runner->AddLeds<WS2812B, LED_PIN, GRB>(sPixels);
#endif
}

}  // namespace jazzlights

#elif JL_IS_CONFIG(PHONE) || JL_IS_CONFIG(ORRERY_LEADER)
namespace jazzlights {
void AddLedsToRunner(FastLedRunner* runner) {}
}  // namespace jazzlights
#endif
