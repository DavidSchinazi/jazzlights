#ifndef JL_ORRERY_COMMON_H
#define JL_ORRERY_COMMON_H

#include "jazzlights/config.h"

#if JL_IS_CONFIG(ORRERY_PLANET) || JL_IS_CONFIG(ORRERY_LEADER)

#include <cstdint>

#include "jazzlights/network/max485_bus.h"

namespace jazzlights {

enum class Planet : BusId {
  Mercury = 4,
  Venus = 5,
  Earth = 6,
  Mars = 7,
  Jupiter = 8,
  Saturn = 9,
  Uranus = 10,
  Neptune = 11,
};

inline constexpr size_t kNumPlanets = 8;

enum class OrreryMessageType : uint8_t {
  SetSpeed = 0x01,
  AckSpeed = 0x02,
};

}  // namespace jazzlights

#endif  // ORRERY

#endif  // JL_ORRERY_COMMON_H
