#ifndef JL_EFFECT_PLANET_H
#define JL_EFFECT_PLANET_H

#include "jazzlights/util/config.h"

#if JL_IS_CONFIG(ORRERY_PLANET)

#include "jazzlights/effect/effect.h"
#include "jazzlights/orrery/orrery_common.h"

namespace jazzlights {

class PlanetEffect : public Effect {
 public:
  static PlanetEffect* Get();
  void SetPlanet(Planet planet);
  void SetHallSensorClosed(bool isClosed);

  // From Effect.
  size_t ContextSize(const Frame& frame) const override;
  void Begin(const Frame& frame) const override;
  void Rewind(const Frame& frame) const override;
  void AfterColors(const Frame& frame) const override;
  CRGB Color(const Frame& frame, const Pixel& px) const override;
  std::string EffectName(PatternBits pattern) const override;

 private:
  struct State {
    bool half;
    bool hall;
    bool hallSensorClosed;
    uint8_t offset;
  };
  PlanetEffect();
  Planet currentPlanet_;
  uint8_t numPixels_;
  bool hallSensorClosed_ = false;
};

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_PLANET)

#endif  // JL_EFFECT_PLANET_H
