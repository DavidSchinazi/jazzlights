#ifndef JL_ORRERY_LEADER_H
#define JL_ORRERY_LEADER_H

#include "jazzlights/config.h"
#include "jazzlights/orrery_common.h"
#include "jazzlights/util/time.h"

#if JL_MAX485_BUS

#include <cstdint>

namespace jazzlights {

class OrreryLeader {
 public:
  static OrreryLeader* Get();
  static constexpr int kNumPlanets = static_cast<int>(Planet::NumPlanets);
  void SetSpeed(Planet planet, int32_t speed);
  int32_t GetSpeed(Planet planet) const;

  void RunLoop(Milliseconds currentTime);

 private:
  OrreryLeader() = default;
  int32_t speeds_[kNumPlanets] = {0};
};

}  // namespace jazzlights

#endif  // JL_MAX485_BUS

#endif  // JL_ORRERY_LEADER_H
