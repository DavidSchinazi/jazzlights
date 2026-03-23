#include "jazzlights/orrery_leader.h"

namespace jazzlights {

void OrreryLeader::SetSpeed(Planet planet, int32_t speed) {
  int planetIndex = static_cast<int>(planet);
  if (planetIndex >= 0 && planetIndex < kNumPlanets) { speeds_[planetIndex] = speed; }
}

int32_t OrreryLeader::GetSpeed(Planet planet) const {
  int planetIndex = static_cast<int>(planet);
  if (planetIndex >= 0 && planetIndex < kNumPlanets) { return speeds_[planetIndex]; }
  return 0;
}

}  // namespace jazzlights
