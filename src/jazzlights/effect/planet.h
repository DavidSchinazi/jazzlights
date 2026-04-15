#ifndef JL_EFFECT_PLANET_H
#define JL_EFFECT_PLANET_H

#include "jazzlights/config.h"

#if JL_IS_CONFIG(ORRERY_PLANET)

#include "jazzlights/effect/effect.h"
#include "jazzlights/orrery_common.h"

namespace jazzlights {

class PlanetEffect : public Effect {
 public:
  static PlanetEffect* Get();
  void SetPlanet(Planet planet);

  // From Effect.
  size_t contextSize(const Frame& frame) const override;
  void begin(const Frame& frame) const override;
  void rewind(const Frame& frame) const override;
  void afterColors(const Frame& frame) const override;
  CRGB color(const Frame& frame, const Pixel& px) const override;
  std::string effectName(PatternBits pattern) const override;

 private:
  PlanetEffect();
  Planet currentPlanet_ = Planet::Earth;
};

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_PLANET)

#endif  // JL_EFFECT_PLANET_H
