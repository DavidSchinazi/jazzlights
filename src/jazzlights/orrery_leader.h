#ifndef JL_ORRERY_LEADER_H
#define JL_ORRERY_LEADER_H

#include "jazzlights/config.h"

#if JL_BUS_LEADER

#include <cstdint>

namespace jazzlights {

enum class Planet : uint8_t { Mercury = 0, Venus, Earth, Mars, Jupiter, Saturn, Uranus, Neptune, NumPlanets };

class OrreryLeader {
 public:
  static constexpr int kNumPlanets = static_cast<int>(Planet::NumPlanets);
  void SetSpeed(Planet planet, int32_t speed);
  int32_t GetSpeed(Planet planet) const;

 private:
  int32_t speeds_[kNumPlanets] = {0};
};

}  // namespace jazzlights

#endif  // JL_BUS_LEADER

#endif  // JL_ORRERY_LEADER_H
