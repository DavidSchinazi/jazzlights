#ifndef JL_EFFECT_CLOUDS_H
#define JL_EFFECT_CLOUDS_H

#include <algorithm>

#include "jazzlights/config.h"
#include "jazzlights/effect/effect.h"
#include "jazzlights/layout/layout_data_clouds.h"

#if JL_IS_CONFIG(CLOUDS)

namespace jazzlights {

class Clouds : public Effect {
 public:
  Clouds() = default;

  std::string effectName(PatternBits /*pattern*/) const override { return "clouds"; }

  size_t contextSize(const Frame& /*frame*/) const override { return sizeof(CloudsState); }

  void begin(const Frame& frame) const override {
    new (state(frame)) CloudsState;  // Default-initialize the state.
    const size_t maxNumStrikes = sizeof(state(frame)->strikes) / sizeof(state(frame)->strikes[0]);
    state(frame)->numStrikes = frame.predictableRandom->GetRandomNumberBetween(maxNumStrikes / 2, maxNumStrikes);
    for (uint8_t s = 0; s < state(frame)->numStrikes; s++) {
      state(frame)->strikes[s].cloudNum = frame.predictableRandom->GetRandomNumberBetween(1, 7);
      state(frame)->strikes[s].startTime = frame.predictableRandom->GetRandomNumberBetween(0, 10000 - StrikeDuration());
    }
  }

  void rewind(const Frame& frame) const override {
    for (uint8_t s = 0; s < state(frame)->numStrikes; s++) {
      if (state(frame)->strikes[s].startTime <= frame.time &&
          frame.time <= state(frame)->strikes[s].startTime + StrikeDuration()) {
        state(frame)->strikes[s].progress =
            static_cast<double>(frame.time - state(frame)->strikes[s].startTime) * 2.0 / StrikeDuration();
      } else {
        state(frame)->strikes[s].progress = -1.0;
      }
    }
  }

  void afterColors(const Frame& /*frame*/) const override {
    static_assert(std::is_trivially_destructible<CloudsState>::value, "CloudsState must be trivially destructible");
  }

  CRGB color(const Frame& frame, const Pixel& px) const override {
    static const CRGB kBrightColor = CRGB(0xFCC97C);
    static const CRGB kDarkColor = CRGB(0x000000);
    static const CRGB kSkyColor = CRGB(0x000020);
    if (px.layout != GetCloudsLayout()) { return kSkyColor; }
    for (uint8_t s = 0; s < state(frame)->numStrikes; s++) {
      const double progress = state(frame)->strikes[s].progress;
      if (progress < 0.5) { continue; }
      if (state(frame)->strikes[s].cloudNum != px.coord.x) { continue; }
      const uint8_t cloudLength = CloudLength(state(frame)->strikes[s].cloudNum);
      if (progress < 1.0) {
        if (px.coord.y < cloudLength * progress) { return kBrightColor; }
      } else {
        if (px.coord.y > cloudLength * (progress - 1.0)) { return kBrightColor; }
      }
    }
    return kDarkColor;
  }

 private:
  static constexpr Milliseconds StrikeDuration() { return 1000; };
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
      Milliseconds startTime;
      double progress;
    };
    LightningStrike strikes[32];
    uint8_t numStrikes;
  };
  CloudsState* state(const Frame& frame) const {
    static_assert(alignof(CloudsState) <= kMaxStateAlignment, "Need to increase kMaxStateAlignment");
    return static_cast<CloudsState*>(frame.context);
  }
};

}  // namespace jazzlights
#endif  // CLOUDS
#endif  // JL_EFFECT_CLOUDS_H
