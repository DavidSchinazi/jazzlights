#include <unity.h>

#include "jazzlights/effects/calibration.h"
#include "jazzlights/effects/clouds.h"
#include "jazzlights/effects/colored_bursts.h"
#include "jazzlights/effects/fairy_wand.h"
#include "jazzlights/effects/flame.h"
#include "jazzlights/effects/follow_strand.h"
#include "jazzlights/effects/glitter.h"
#include "jazzlights/effects/glow.h"
#include "jazzlights/effects/hiphotic.h"
#include "jazzlights/effects/mapping.h"
#include "jazzlights/effects/metaballs.h"
#include "jazzlights/effects/plasma.h"
#include "jazzlights/effects/rainbow.h"
#include "jazzlights/effects/solid.h"
#include "jazzlights/effects/sync_test.h"
#include "jazzlights/effects/the_matrix.h"
#include "jazzlights/effects/threesine.h"
#include "jazzlights/layouts/matrix.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif  // ARDUINO

namespace jazzlights {

void test_pattern(const Effect& effect) {
  Matrix layout(1, 1);
  PredictableRandom predictableRandom;
  XYIndexStore xyIndexStore;
  xyIndexStore.IngestLayout(&layout);
  xyIndexStore.Finalize(jazzlights::bounds(layout));
  Frame frame;
  frame.pattern = 0;
  frame.predictableRandom = &predictableRandom;
  frame.xyIndexStore = &xyIndexStore;
  frame.viewport = {};
  frame.context = nullptr;
  frame.time = 33;
  frame.pixelCount = layout.pixelCount();
  TEST_ASSERT_NOT_EQUAL("", effect.effectName(frame.pattern));
  size_t effectContextSize = effect.contextSize(frame);
  if ((effectContextSize % kMaxStateAlignment) != 0) {
    effectContextSize += kMaxStateAlignment - (effectContextSize % kMaxStateAlignment);
  }
  frame.context = aligned_alloc(kMaxStateAlignment, effectContextSize);
  TEST_ASSERT_NOT_NULL(frame.context);
  predictableRandom.ResetWithFrameStart(frame, effect.effectName(frame.pattern).c_str());
  effect.begin(frame);
  predictableRandom.ResetWithFrameTime(frame, effect.effectName(frame.pattern).c_str());
  effect.rewind(frame);
  Pixel px;
  px.layout = &layout;
  px.index = 0;
  px.coord = {0.0, 0.0};
  CRGB col = effect.color(frame, px);
  effect.afterColors(frame);
  free(frame.context);
  frame.context = nullptr;
}

void test_spin_pattern(void) {
  static const SpinPlasma spin_pattern;
  test_pattern(spin_pattern);
}
void test_hiphotic_pattern(void) {
  static const Hiphotic hiphotic_pattern;
  test_pattern(hiphotic_pattern);
}
void test_metaballs_pattern(void) {
  static const Metaballs metaballs_pattern;
  test_pattern(metaballs_pattern);
}
void test_colored_bursts_pattern(void) {
  static const ColoredBursts colored_bursts_pattern;
  test_pattern(colored_bursts_pattern);
}
void test_flame_pattern(void) {
  static const Flame flame_pattern;
  test_pattern(flame_pattern);
}
void test_glitter_pattern(void) {
  static const Glitter glitter_pattern;
  test_pattern(glitter_pattern);
}
void test_thematrix_pattern(void) {
  static const TheMatrix thematrix_pattern;
  test_pattern(thematrix_pattern);
}
void test_rainbow_pattern(void) {
  static const Rainbow rainbow_pattern;
  test_pattern(rainbow_pattern);
}
void test_threesine_pattern(void) {
  static const FunctionalEffect threesine_pattern = threesine();
  test_pattern(threesine_pattern);
}
void test_follow_strand_effect(void) {
  static const FunctionalEffect follow_strand_effect = follow_strand();
  test_pattern(follow_strand_effect);
}
void test_mapping_effect(void) {
  static const FunctionalEffect mapping_effect = mapping();
  test_pattern(mapping_effect);
}
void test_coloring_effect(void) {
  static const FunctionalEffect coloring_effect = coloring();
  test_pattern(coloring_effect);
}
void test_calibration_effect(void) {
  static const FunctionalEffect calibration_effect = calibration();
  test_pattern(calibration_effect);
}
void test_sync_test_effect(void) {
  static const FunctionalEffect sync_test_effect = sync_test();
  test_pattern(sync_test_effect);
}
void test_black_effect(void) {
  static const FunctionalEffect black_effect = solid(CRGB::Black, "black");
  test_pattern(black_effect);
}
void test_red_effect(void) {
  static const FunctionalEffect red_effect = solid(CRGB::Black, "red");
  test_pattern(red_effect);
}
void test_green_effect(void) {
  static const FunctionalEffect green_effect = solid(CRGB::Green, "green");
  test_pattern(green_effect);
}
void test_blue_effect(void) {
  static const FunctionalEffect blue_effect = solid(CRGB::Blue, "blue");
  test_pattern(blue_effect);
}
void test_purple_effect(void) {
  static const FunctionalEffect purple_effect = solid(CRGB::Purple, "purple");
  test_pattern(purple_effect);
}
void test_cyan_effect(void) {
  static const FunctionalEffect cyan_effect = solid(CRGB::Cyan, "cyan");
  test_pattern(cyan_effect);
}
void test_yellow_effect(void) {
  static const FunctionalEffect yellow_effect = solid(CRGB::Yellow, "yellow");
  test_pattern(yellow_effect);
}
void test_white_effect(void) {
  static const FunctionalEffect white_effect = solid(CRGB::White, "white");
  test_pattern(white_effect);
}
void testred_glow_effect(void) {
  static const FunctionalEffect red_glow_effect = glow(CRGB::Black, "glow-red");
  test_pattern(red_glow_effect);
}
void test_green_glow_effect(void) {
  static const FunctionalEffect green_glow_effect = glow(CRGB::Green, "glow-green");
  test_pattern(green_glow_effect);
}
void test_blue_glow_effect(void) {
  static const FunctionalEffect blue_glow_effect = glow(CRGB::Blue, "glow-blue");
  test_pattern(blue_glow_effect);
}
void test_purple_glow_effect(void) {
  static const FunctionalEffect purple_glow_effect = glow(CRGB::Purple, "glow-purple");
  test_pattern(purple_glow_effect);
}
void test_cyan_glow_effect(void) {
  static const FunctionalEffect cyan_glow_effect = glow(CRGB::Cyan, "glow-cyan");
  test_pattern(cyan_glow_effect);
}
void test_yellow_glow_effect(void) {
  static const FunctionalEffect yellow_glow_effect = glow(CRGB::Yellow, "glow-yellow");
  test_pattern(yellow_glow_effect);
}
void test_white_glow_effect(void) {
  static const FunctionalEffect white_glow_effect = glow(CRGB::White, "glow-white");
  test_pattern(white_glow_effect);
}

void run_unity_tests() {
  UNITY_BEGIN();
  RUN_TEST(test_spin_pattern);
  RUN_TEST(test_hiphotic_pattern);
  RUN_TEST(test_metaballs_pattern);
  RUN_TEST(test_colored_bursts_pattern);
  RUN_TEST(test_flame_pattern);
  RUN_TEST(test_glitter_pattern);
  RUN_TEST(test_thematrix_pattern);
  RUN_TEST(test_rainbow_pattern);
  RUN_TEST(test_threesine_pattern);
  RUN_TEST(test_follow_strand_effect);
  RUN_TEST(test_mapping_effect);
  RUN_TEST(test_coloring_effect);
  RUN_TEST(test_calibration_effect);
  RUN_TEST(test_sync_test_effect);
  RUN_TEST(test_black_effect);
  RUN_TEST(test_red_effect);
  RUN_TEST(test_green_effect);
  RUN_TEST(test_blue_effect);
  RUN_TEST(test_purple_effect);
  RUN_TEST(test_cyan_effect);
  RUN_TEST(test_yellow_effect);
  RUN_TEST(test_white_effect);
  RUN_TEST(test_green_glow_effect);
  RUN_TEST(test_blue_glow_effect);
  RUN_TEST(test_purple_glow_effect);
  RUN_TEST(test_cyan_glow_effect);
  RUN_TEST(test_yellow_glow_effect);
  RUN_TEST(test_white_glow_effect);
  UNITY_END();
}

}  // namespace jazzlights

void setUp(void) {}

void tearDown(void) {}

#ifdef ARDUINO

void setup() {
  Serial.begin(115200);
  // The 2s delay is required if board doesn't support software reset via Serial.DTR/RTS.
  delay(2000);

  jazzlights::run_unity_tests();
}

void loop() {}

#else  // ARDUINO

int main(int /*argc*/, char** /*argv*/) {
  jazzlights::run_unity_tests();
  return 0;
}

#endif  // ARDUINO
