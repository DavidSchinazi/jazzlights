#ifndef JL_ORRERY_LEADER_H
#define JL_ORRERY_LEADER_H

#include "jazzlights/config.h"

#if JL_IS_CONFIG(ORRERY_LEADER)

#include <cstdint>

#include "jazzlights/network/max485_bus.h"
#include "jazzlights/orrery_common.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

class OrreryLeader {
 public:
  static OrreryLeader* Get();
  void SetSpeed(Planet planet, int32_t speed);
  int32_t GetSpeed(Planet planet) const;

  void RunLoop(Milliseconds currentTime);

 private:
  OrreryLeader();
  int32_t speeds_[kNumPlanets] = {0};
  Max485BusLeader max485BusLeader_;
};

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_LEADER)

#endif  // JL_ORRERY_LEADER_H
