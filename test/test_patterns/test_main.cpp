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

void TestPattern(const Effect& effect) {
  Matrix layout(1, 1);
  NoOpRenderer renderer;
  Strand strand = {layout, renderer, 0};
  PredictableRandom predictableRandom;
  XYIndexStore xyIndexStore;
  xyIndexStore.IngestLayout(&layout);
  xyIndexStore.Finalize(jazzlights::Bounds(layout));
  Frame frame;
  frame.pattern = 0;
  frame.predictableRandom = &predictableRandom;
  frame.xyIndexStore = &xyIndexStore;
  frame.viewport = {};
  frame.context = nullptr;
  frame.time = 33;
  frame.pixelCount = layout.PixelCount();
  TEST_ASSERT_NOT_EQUAL("", effect.EffectName(frame.pattern));
  size_t effectContextSize = effect.ContextSize(frame);
  if ((effectContextSize % kMaxStateAlignment) != 0) {
    effectContextSize += kMaxStateAlignment - (effectContextSize % kMaxStateAlignment);
  }
  frame.context = aligned_alloc(kMaxStateAlignment, effectContextSize);
  TEST_ASSERT_NOT_NULL(frame.context);
  predictableRandom.ResetWithFrameStart(frame, effect.EffectName(frame.pattern).c_str());
  effect.Begin(frame);
  predictableRandom.ResetWithFrameTime(frame, effect.EffectName(frame.pattern).c_str());
  effect.Rewind(frame);
  Pixel px;
  px.strand = &strand;
  px.strandIndex = 0;
  px.cumulativeIndex = 0;
  px.coord = {0.0, 0.0};
  CRGB col = effect.Color(frame, px);
  effect.AfterColors(frame);
  free(frame.context);
  frame.context = nullptr;
}

void TestSpinPattern() {
  static const SpinPlasma kSpinPattern;
  TestPattern(kSpinPattern);
}
void TestHiphoticPattern() {
  static const Hiphotic kHiphoticPattern;
  TestPattern(kHiphoticPattern);
}
void TestMetaballsPattern() {
  static const Metaballs kMetaballsPattern;
  TestPattern(kMetaballsPattern);
}
void TestColoredBurstsPattern() {
  static const ColoredBursts kColoredBurstsPattern;
  TestPattern(kColoredBurstsPattern);
}
void TestFlamePattern() {
  static const Flame kFlamePattern;
  TestPattern(kFlamePattern);
}
void TestGlitterPattern() {
  static const Glitter kGlitterPattern;
  TestPattern(kGlitterPattern);
}
void TestThematrixPattern() {
  static const TheMatrix kTheMatrixPattern;
  TestPattern(kTheMatrixPattern);
}
void TestRingsPattern() {
  static const Rings kRingsPattern;
  TestPattern(kRingsPattern);
}
void TestThreesinePattern() {
  static const FunctionalEffect kThreesinePattern = Threesine();
  TestPattern(kThreesinePattern);
}
void TestFollowStrandEffect() {
  static const FunctionalEffect kFollowStrandEffect = FollowStrand();
  TestPattern(kFollowStrandEffect);
}
void TestMappingEffect() {
  static const FunctionalEffect kMappingEffect = Mapping();
  TestPattern(kMappingEffect);
}
void TestColoringEffect() {
  static const FunctionalEffect kColoringEffect = Coloring();
  TestPattern(kColoringEffect);
}
void TestCalibrationEffect() {
  static const FunctionalEffect kCalibrationEffect = Calibration();
  TestPattern(kCalibrationEffect);
}
void TestSyncTestEffect() {
  static const FunctionalEffect kSyncTestEffect = SyncTest();
  TestPattern(kSyncTestEffect);
}
void TestBlackEffect() {
  static const FunctionalEffect kBlackEffect = Solid(CRGB::Black, "black");
  TestPattern(kBlackEffect);
}
void TestRedEffect() {
  static const FunctionalEffect kRedEffect = Solid(CRGB::Red, "red");
  TestPattern(kRedEffect);
}
void TestGreenEffect() {
  static const FunctionalEffect kGreenEffect = Solid(CRGB::Green, "green");
  TestPattern(kGreenEffect);
}
void TestBlueEffect() {
  static const FunctionalEffect kBlueEffect = Solid(CRGB::Blue, "blue");
  TestPattern(kBlueEffect);
}
void TestPurpleEffect() {
  static const FunctionalEffect kPurpleEffect = Solid(CRGB::Purple, "purple");
  TestPattern(kPurpleEffect);
}
void TestCyanEffect() {
  static const FunctionalEffect kCyanEffect = Solid(CRGB::Cyan, "cyan");
  TestPattern(kCyanEffect);
}
void TestYellowEffect() {
  static const FunctionalEffect kYellowEffect = Solid(CRGB::Yellow, "yellow");
  TestPattern(kYellowEffect);
}
void TestWhiteEffect() {
  static const FunctionalEffect kWhiteEffect = Solid(CRGB::White, "white");
  TestPattern(kWhiteEffect);
}
void TestRedGlowEffect() {
  static const FunctionalEffect kRedGlowEffect = Glow(CRGB::Red, "glow-red");
  TestPattern(kRedGlowEffect);
}
void TestGreenGlowEffect() {
  static const FunctionalEffect kGreenGlowEffect = Glow(CRGB::Green, "glow-green");
  TestPattern(kGreenGlowEffect);
}
void TestBlueGlowEffect() {
  static const FunctionalEffect kBlueGlowEffect = Glow(CRGB::Blue, "glow-blue");
  TestPattern(kBlueGlowEffect);
}
void TestPurpleGlowEffect() {
  static const FunctionalEffect kPurpleGlowEffect = Glow(CRGB::Purple, "glow-purple");
  TestPattern(kPurpleGlowEffect);
}
void TestCyanGlowEffect() {
  static const FunctionalEffect kCyanGlowEffect = Glow(CRGB::Cyan, "glow-cyan");
  TestPattern(kCyanGlowEffect);
}
void TestYellowGlowEffect() {
  static const FunctionalEffect kYellowGlowEffect = Glow(CRGB::Yellow, "glow-yellow");
  TestPattern(kYellowGlowEffect);
}
void TestWhiteGlowEffect() {
  static const FunctionalEffect kWhiteGlowEffect = Glow(CRGB::White, "glow-white");
  TestPattern(kWhiteGlowEffect);
}

void RunUnityTests() {
  UNITY_BEGIN();
  RUN_TEST(TestSpinPattern);
  RUN_TEST(TestHiphoticPattern);
  RUN_TEST(TestMetaballsPattern);
  RUN_TEST(TestColoredBurstsPattern);
  RUN_TEST(TestFlamePattern);
  RUN_TEST(TestGlitterPattern);
  RUN_TEST(TestThematrixPattern);
  RUN_TEST(TestRingsPattern);
  RUN_TEST(TestThreesinePattern);
  RUN_TEST(TestFollowStrandEffect);
  RUN_TEST(TestMappingEffect);
  RUN_TEST(TestColoringEffect);
  RUN_TEST(TestCalibrationEffect);
  RUN_TEST(TestSyncTestEffect);
  RUN_TEST(TestBlackEffect);
  RUN_TEST(TestRedEffect);
  RUN_TEST(TestGreenEffect);
  RUN_TEST(TestBlueEffect);
  RUN_TEST(TestPurpleEffect);
  RUN_TEST(TestCyanEffect);
  RUN_TEST(TestYellowEffect);
  RUN_TEST(TestWhiteEffect);
  RUN_TEST(TestRedGlowEffect);
  RUN_TEST(TestGreenGlowEffect);
  RUN_TEST(TestBlueGlowEffect);
  RUN_TEST(TestPurpleGlowEffect);
  RUN_TEST(TestCyanGlowEffect);
  RUN_TEST(TestYellowGlowEffect);
  RUN_TEST(TestWhiteGlowEffect);
  UNITY_END();
}

}  // namespace jazzlights

void setUp() {}

void tearDown() {}

#ifdef ESP32

void setup() { jazzlights::RunUnityTests(); }

void loop() {}

#else  // ESP32

int main(int /*argc*/, char** /*argv*/) {
  jazzlights::RunUnityTests();
  return 0;
}

#endif  // ESP32
