#ifndef JL_ORRERY_PLANET_H
#define JL_ORRERY_PLANET_H

#include "jazzlights/config.h"
#include "jazzlights/util/time.h"

#if JL_MAX485_BUS

namespace jazzlights {

class OrreryPlanet {
 public:
  static OrreryPlanet* Get();
  void RunLoop(Milliseconds currentTime);

 private:
  OrreryPlanet() = default;
};

}  // namespace jazzlights

#endif  // JL_MAX485_BUS

#endif  // JL_ORRERY_PLANET_H
