#ifndef JL_ORRERY_LEADER_H
#define JL_ORRERY_LEADER_H

#include "jazzlights/config.h"

#if JL_IS_CONFIG(ORRERY_LEADER)

#include <cstdint>
#include <unordered_map>

#include "jazzlights/network/max485_bus.h"
#include "jazzlights/orrery_common.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

class OrreryLeader {
 public:
  static OrreryLeader* Get();
  void SetSpeed(Planet planet, int32_t speed);
  int32_t GetSpeed(Planet planet) const;
  void SetBrightness(Planet planet, uint8_t brightness);
  uint8_t GetBrightness(Planet planet) const;
  std::optional<uint32_t> GetTimeHallSensorLastOpened(Planet planet) const;

  void RunLoop(Milliseconds currentTime);

 private:
  OrreryLeader();
  void SendMessage(Planet planet);
  const uint32_t bootId_;
  uint32_t nextSequenceNumber_ = 0;
  std::unordered_map<Planet, OrreryMessage> messages_;
  std::unordered_map<Planet, OrreryMessage> responses_;
  Max485BusLeader max485BusLeader_;
};

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_LEADER)

#endif  // JL_ORRERY_LEADER_H
