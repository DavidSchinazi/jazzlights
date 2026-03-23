#ifndef JL_ORRERY_LEADER_H
#define JL_ORRERY_LEADER_H

#include "jazzlights/config.h"
#include "jazzlights/util/time.h"

#if JL_MAX485_BUS

#include <cstdint>

namespace jazzlights {

enum class Planet : uint8_t { Mercury = 0, Venus, Earth, Mars, Jupiter, Saturn, Uranus, Neptune, NumPlanets };

enum class OrreryMessageType : uint8_t {
  SetSpeed = 0x01,
  AckSpeed = 0x02,
};

struct OrreryMessage {
  OrreryMessageType type;
  uint8_t planetIndex;
  int32_t speed;
} __attribute__((packed));

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
