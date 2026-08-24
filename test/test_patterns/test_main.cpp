#include <unity.h>

#include "jazzlights/effect/calibration.h"
#include "jazzlights/effect/clouds.h"
#include "jazzlights/effect/colored_bursts.h"
#include "jazzlights/effect/fairy_wand.h"
#include "jazzlights/effect/flame.h"
#include "jazzlights/effect/follow_strand.h"
#include "jazzlights/effect/glitter.h"
#include "jazzlights/effect/glow.h"
#include "jazzlights/effect/hiphotic.h"
#include "jazzlights/effect/mapping.h"
#include "jazzlights/effect/metaballs.h"
#include "jazzlights/effect/plasma.h"
#include "jazzlights/effect/rings.h"
#include "jazzlights/effect/solid.h"
#include "jazzlights/effect/sync_test.h"
#include "jazzlights/effect/the_matrix.h"
#include "jazzlights/effect/threesine.h"
#include "jazzlights/layout/matrix.h"
#include "jazzlights/render/predictable_random.h"
#include "jazzlights/render/renderer.h"

namespace jazzlights {

class NoOpRenderer : public Renderer {
 public:
  void RenderPixel(size_t /*index*/, CRGB /*color*/) override {}
};

void test_pattern(const Effect& effect) {
  Matrix layout(1, 1);
  NoOpRenderer renderer;
  Strand strand = {layout, renderer, 0};
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
  px.strand = &strand;
  px.strandIndex = 0;
  px.cumulativeIndex = 0;
  px.coord = {0.0, 0.0};
  CRGB col = effect.color(frame, px);
  effect.afterColors(frame);
  free(frame.context);
  frame.context = nullptr;
}

void test_spin_pattern() {
  static const SpinPlasma kSpinPattern;
  test_pattern(kSpinPattern);
}
void test_hiphotic_pattern() {
  static const Hiphotic kHiphoticPattern;
  test_pattern(kHiphoticPattern);
}
void test_metaballs_pattern() {
  static const Metaballs kMetaballsPattern;
  test_pattern(kMetaballsPattern);
}
void test_colored_bursts_pattern() {
  static const ColoredBursts kColoredBurstsPattern;
  test_pattern(kColoredBurstsPattern);
}
void test_flame_pattern() {
  static const Flame kFlamePattern;
  test_pattern(kFlamePattern);
}
void test_glitter_pattern() {
  static const Glitter kGlitterPattern;
  test_pattern(kGlitterPattern);
}
void test_thematrix_pattern() {
  static const TheMatrix kTheMatrixPattern;
  test_pattern(kTheMatrixPattern);
}
void test_rings_pattern() {
  static const Rings kRingsPattern;
  test_pattern(kRingsPattern);
}
void test_threesine_pattern() {
  static const FunctionalEffect kThreesinePattern = threesine();
  test_pattern(kThreesinePattern);
}
void test_follow_strand_effect() {
  static const FunctionalEffect kFollowStrandEffect = follow_strand();
  test_pattern(kFollowStrandEffect);
}
void test_mapping_effect() {
  static const FunctionalEffect kMappingEffect = mapping();
  test_pattern(kMappingEffect);
}
void test_coloring_effect() {
  static const FunctionalEffect kColoringEffect = coloring();
  test_pattern(kColoringEffect);
}
void test_calibration_effect() {
  static const FunctionalEffect kCalibrationEffect = calibration();
  test_pattern(kCalibrationEffect);
}
void test_sync_test_effect() {
  static const FunctionalEffect kSyncTestEffect = sync_test();
  test_pattern(kSyncTestEffect);
}
void test_black_effect() {
  static const FunctionalEffect kBlackEffect = solid(CRGB::Black, "black");
  test_pattern(kBlackEffect);
}
void test_red_effect() {
  static const FunctionalEffect kRedEffect = solid(CRGB::Black, "red");
  test_pattern(kRedEffect);
}
void test_green_effect() {
  static const FunctionalEffect kGreenEffect = solid(CRGB::Green, "green");
  test_pattern(kGreenEffect);
}
void test_blue_effect() {
  static const FunctionalEffect kBlueEffect = solid(CRGB::Blue, "blue");
  test_pattern(kBlueEffect);
}
void test_purple_effect() {
  static const FunctionalEffect kPurpleEffect = solid(CRGB::Purple, "purple");
  test_pattern(kPurpleEffect);
}
void test_cyan_effect() {
  static const FunctionalEffect kCyanEffect = solid(CRGB::Cyan, "cyan");
  test_pattern(kCyanEffect);
}
void test_yellow_effect() {
  static const FunctionalEffect kYellowEffect = solid(CRGB::Yellow, "yellow");
  test_pattern(kYellowEffect);
}
void test_white_effect() {
  static const FunctionalEffect kWhiteEffect = solid(CRGB::White, "white");
  test_pattern(kWhiteEffect);
}
void testred_glow_effect() {
  static const FunctionalEffect kRedGlowEffect = glow(CRGB::Black, "glow-red");
  test_pattern(kRedGlowEffect);
}
void test_green_glow_effect() {
  static const FunctionalEffect kGreenGlowEffect = glow(CRGB::Green, "glow-green");
  test_pattern(kGreenGlowEffect);
}
void test_blue_glow_effect() {
  static const FunctionalEffect kBlueGlowEffect = glow(CRGB::Blue, "glow-blue");
  test_pattern(kBlueGlowEffect);
}
void test_purple_glow_effect() {
  static const FunctionalEffect kPurpleGlowEffect = glow(CRGB::Purple, "glow-purple");
  test_pattern(kPurpleGlowEffect);
}
void test_cyan_glow_effect() {
  static const FunctionalEffect kCyanGlowEffect = glow(CRGB::Cyan, "glow-cyan");
  test_pattern(kCyanGlowEffect);
}
void test_yellow_glow_effect() {
  static const FunctionalEffect kYellowGlowEffect = glow(CRGB::Yellow, "glow-yellow");
  test_pattern(kYellowGlowEffect);
}
void test_white_glow_effect() {
  static const FunctionalEffect kWhiteGlowEffect = glow(CRGB::White, "glow-white");
  test_pattern(kWhiteGlowEffect);
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
  RUN_TEST(test_rings_pattern);
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

void setUp() {}

void tearDown() {}

#ifdef ESP32

void setup() { jazzlights::run_unity_tests(); }

void loop() {}

#else  // ESP32

int main(int /*argc*/, char** /*argv*/) {
  jazzlights::run_unity_tests();
  return 0;
}

#endif  // ESP32
