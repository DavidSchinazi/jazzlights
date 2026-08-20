#ifndef JL_ORRERY_COMMON_H
#define JL_ORRERY_COMMON_H

#include <cstdint>
#include <optional>
#include <string>

#include "jazzlights/config.h"
#include "jazzlights/types.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

enum class OrreryScene : uint8_t {
  Paused = 0,
  Realistic = 1,
  Align = 2,
  Silly = 3,
  FocusMercury = 4,
  FocusVenus = 5,
  FocusEarth = 6,
  FocusMars = 7,
  FocusJupiter = 8,
  FocusSaturn = 9,
  FocusUranus = 10,
  FocusNeptune = 11,
  FocusSun = 12,
  MercuryRetrograde = 13,

  kInvalidScene = 100,
  kMinScene = Paused,
  kMaxScene = MercuryRetrograde,
};

const char* OrrerySceneToString(OrreryScene scene);

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
  Sun = 12,     // Yes, the sun is a planet. Deal with it.
  Pluto = 254,  // And pluto is... complicated.
  All = 255,
};

inline constexpr size_t kNumPlanetsWithoutSun = 8;
inline constexpr size_t kNumPlanets = 9;
inline constexpr int32_t kDefaultPlanetSpeed = 1000;
inline constexpr uint8_t kDefaultPlanetBrightness = 64;
inline constexpr PatternBits kPlanetPattern = 0x0000FE00;
inline constexpr PatternBits kPlanetPatternHalfBit = 0x80000000;
inline constexpr PatternBits kPlanetPatternHallSensorBit = 0x40000000;
inline constexpr size_t kPlanetPatternOffsetShift = 16;
inline constexpr PatternBits kPlanetPatternOffsetMask = 0xFF;
inline constexpr Precedence kDefaultPlanetBasePrecedence = 100;
inline constexpr Precedence kDefaultPlanetPrecedenceGain = 100;
inline constexpr Precedence kOrreryLeaderBasePrecedence = 222;
inline constexpr uint32_t kOrreryPositionNone = 0xFFFFFFFF;
inline constexpr int32_t kOrrerySpeedDisable = INT32_MIN;

enum class OrreryMessageType : uint8_t {
  LeaderCommand = 0x01,
  FollowerResponse = 0x02,
};

#if JL_IS_CONFIG(ORRERY_PLANET) || JL_IS_CONFIG(ORRERY_LEADER) || JL_IS_CONTROLLER(CORE2AWS) || \
    JL_IS_CONTROLLER(CORES3) || PIO_UNIT_TESTING

struct OrreryMessage {
  OrreryMessageType type;
  uint32_t leaderBootId;
  uint32_t leaderSequenceNumber;
  std::optional<int32_t> speed;
  std::optional<uint32_t> position;
  std::optional<uint32_t> calibration;
  std::optional<Milliseconds> timeHallSensorLastOpened;
  std::optional<Milliseconds> timeHallSensorLastClosed;
  std::optional<Milliseconds> lastOpenDuration;
  std::optional<Milliseconds> lastClosedDuration;
  std::optional<PatternBits> ledPattern;
  std::optional<uint8_t> ledBrightness;
  std::optional<Precedence> ledBasePrecedence;
  std::optional<Precedence> ledPrecedenceGain;

  // IMPORTANT: If additional data is added to OrreryMessage, kMaxMessageLength needs to be adjusted in
  // Max485BusHandler.

  bool operator==(const OrreryMessage& other) const {
    return type == other.type && leaderBootId == other.leaderBootId &&
           leaderSequenceNumber == other.leaderSequenceNumber && speed == other.speed && position == other.position &&
           calibration == other.calibration && timeHallSensorLastOpened == other.timeHallSensorLastOpened &&
           timeHallSensorLastClosed == other.timeHallSensorLastClosed && lastOpenDuration == other.lastOpenDuration &&
           lastClosedDuration == other.lastClosedDuration && ledPattern == other.ledPattern &&
           ledBrightness == other.ledBrightness && ledBasePrecedence == other.ledBasePrecedence &&
           ledPrecedenceGain == other.ledPrecedenceGain;
  }
  bool operator!=(const OrreryMessage& other) const { return !(*this == other); }
};

bool WriteOrreryMessage(const OrreryMessage& msg, NetworkWriter& writer);
bool ReadOrreryMessage(NetworkReader& reader, OrreryMessage* msg);
std::string OrreryMessageToString(const OrreryMessage& msg);
const char* GetPlanetName(Planet planet);

#endif  // ORRERY

}  // namespace jazzlights

#endif  // JL_ORRERY_COMMON_H
