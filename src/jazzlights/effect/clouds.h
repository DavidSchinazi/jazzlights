#ifndef JL_EFFECT_CLOUDS_H
#define JL_EFFECT_CLOUDS_H

#include <algorithm>

#include "jazzlights/effect/effect.h"
#include "jazzlights/layout/layout_data_clouds.h"
#include "jazzlights/util/config.h"

#if JL_IS_CONFIG(CLOUDS)

namespace jazzlights {

class Clouds : public Effect {
 public:
  Clouds() = default;

  std::string EffectName(PatternBits /*pattern*/) const override { return "clouds"; }

  size_t ContextSize(const Frame& /*frame*/) const override { return sizeof(CloudsState); }

  void Begin(const Frame& frame) const override {
    new (State(frame)) CloudsState;  // Default-initialize the state.
    const size_t maxNumStrikes = sizeof(State(frame)->strikes) / sizeof(State(frame)->strikes[0]);
    State(frame)->numStrikes = frame.predictableRandom->GetRandomNumberBetween(maxNumStrikes / 2, maxNumStrikes);
    for (uint8_t s = 0; s < State(frame)->numStrikes; s++) {
      State(frame)->strikes[s].cloudNum = frame.predictableRandom->GetRandomNumberBetween(1, 7);
      State(frame)->strikes[s].startTime = frame.predictableRandom->GetRandomNumberBetween(0, 10000 - StrikeDuration());
    }
  }

  void Rewind(const Frame& frame) const override {
    for (uint8_t s = 0; s < State(frame)->numStrikes; s++) {
      if (State(frame)->strikes[s].startTime <= frame.time &&
          frame.time <= State(frame)->strikes[s].startTime + StrikeDuration()) {
        State(frame)->strikes[s].progress =
            static_cast<double>(frame.time - State(frame)->strikes[s].startTime) * 2.0 / StrikeDuration();
      } else {
        State(frame)->strikes[s].progress = -1.0;
      }
    }
  }

  void AfterColors(const Frame& /*frame*/) const override {
    static_assert(std::is_trivially_destructible<CloudsState>::value, "CloudsState must be trivially destructible");
  }

  CRGB Color(const Frame& frame, const Pixel& px) const override {
    static const CRGB kBrightColor = CRGB(0xFCC97C);
    static const CRGB kDarkColor = CRGB(0x000000);
    static const CRGB kSkyColor = CRGB(0x000020);
    if (&px.strand->layout != GetCloudsLayout()) { return kSkyColor; }
    for (uint8_t s = 0; s < State(frame)->numStrikes; s++) {
      const double progress = State(frame)->strikes[s].progress;
      if (progress < 0.5) { continue; }
      if (State(frame)->strikes[s].cloudNum != px.coord.x) { continue; }
      const uint8_t cloudLength = CloudLength(State(frame)->strikes[s].cloudNum);
      if (progress < 1.0) {
        if (px.coord.y < cloudLength * progress) { return kBrightColor; }
      } else {
        if (px.coord.y > cloudLength * (progress - 1.0)) { return kBrightColor; }
      }
    }
    return kDarkColor;
  }

 private:
  static constexpr FrameTimeMs StrikeDuration() { return 1000; };
  static uint8_t CloudLength(uint8_t cloudNum) {
    switch (cloudNum) {
      case 1: return 17;
      case 2: return 15;
      case 3: return 13;
      case 4: return 11;
      case 5: return 14;
      case 6: return 7;
      case 7: return 10;
    }
    return 0;
  }
  struct CloudsState {
    struct LightningStrike {
      uint8_t cloudNum;
      FrameTimeMs startTime;
      double progress;
    };
    LightningStrike strikes[32];
    uint8_t numStrikes;
  };
  CloudsState* State(const Frame& frame) const {
    static_assert(alignof(CloudsState) <= kMaxStateAlignment, "Need to increase kMaxStateAlignment");
    return static_cast<CloudsState*>(frame.context);
  }
};

}  // namespace jazzlights
#endif  // CLOUDS
#endif  // JL_EFFECT_CLOUDS_H
