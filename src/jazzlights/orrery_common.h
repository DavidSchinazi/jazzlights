#ifndef JL_ORRERY_COMMON_H
#define JL_ORRERY_COMMON_H

#include "jazzlights/config.h"

#if JL_IS_CONFIG(ORRERY_PLANET) || JL_IS_CONFIG(ORRERY_LEADER) || PIO_UNIT_TESTING

#include <cstdint>
#include <optional>

#include "jazzlights/network/max485_bus.h"
#include "jazzlights/types.h"

namespace jazzlights {

#if !JL_MAX485_BUS
using BusId = uint8_t;
#endif

class NetworkReader;
class NetworkWriter;

enum class Planet : BusId {
  Mercury = 4,
  Venus = 5,
  Earth = 6,
  Mars = 7,
  Jupiter = 8,
  Saturn = 9,
  Uranus = 10,
  Neptune = 11,
  Sun = 12,  // Yes, the sun is a planet. Deal with it.
};

inline constexpr size_t kNumPlanets = 9;
inline constexpr int32_t kDefaultPlanetSpeed = 10000;
inline constexpr uint8_t kDefaultPlanetBrightness = 64;

enum class OrreryMessageType : uint8_t {
  LeaderCommand = 0x01,
  FollowerResponse = 0x02,
};

struct OrreryMessage {
  uint32_t leaderBootId;
  uint32_t leaderSequenceNumber;
  std::optional<int32_t> speed;
  std::optional<uint32_t> position;
  std::optional<uint32_t> calibration;
  std::optional<PatternBits> ledPattern;
  std::optional<uint8_t> ledBrightness;
  std::optional<Precedence> ledPrecedence;
};

bool WriteOrreryMessage(OrreryMessageType type, const OrreryMessage& msg, NetworkWriter& writer);
bool ReadOrreryMessage(NetworkReader& reader, OrreryMessageType* type, OrreryMessage* msg);
const char* GetPlanetName(Planet planet);

}  // namespace jazzlights

#endif  // ORRERY

#endif  // JL_ORRERY_COMMON_H
