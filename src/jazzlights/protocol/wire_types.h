#ifndef JL_PROTOCOL_WIRE_TYPES_H
#define JL_PROTOCOL_WIRE_TYPES_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>

#include "jazzlights/util/config.h"
#include "jazzlights/util/time.h"
#include "jazzlights/util/types.h"

namespace jazzlights {

inline constexpr Microseconds kEffectDuration = 10 * kMicrosecondsPerSecond;  // 10s.

enum class NetworkType {
  kLeading,
  kBLE,
  kWiFi,
  kEthernet,
  kOther,
};

const char* NetworkTypeToString(NetworkType type);

// NetworkId 0 is a sentinel value that cannot be assigned to a network. It can mean that we generated the update
// instead of having received it.
using NetworkId = uint32_t;

#define DEVICE_ID_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define DEVICE_ID_HEX(addr) (addr)(0), (addr)(1), (addr)(2), (addr)(3), (addr)(4), (addr)(5)

class NetworkDeviceId {
 public:
  explicit NetworkDeviceId() { memset(&data_[0], 0, kNetworkDeviceIdSize); }
  explicit NetworkDeviceId(const uint8_t* data) { memcpy(&data_[0], data, kNetworkDeviceIdSize); }
  NetworkDeviceId(const NetworkDeviceId& other) : NetworkDeviceId(&other.data_[0]) {}
  NetworkDeviceId& operator=(const NetworkDeviceId& other) {
    memcpy(&data_[0], &other.data_[0], kNetworkDeviceIdSize);
    return *this;
  }
  uint8_t operator()(uint8_t i) const { return data_[i]; }
  int Compare(const NetworkDeviceId& other) const { return memcmp(&data_[0], &other.data_[0], kNetworkDeviceIdSize); }
  void WriteTo(uint8_t* data) const { memcpy(data, &data_[0], kNetworkDeviceIdSize); }
  void ReadFrom(const uint8_t* data) { memcpy(&data_[0], data, kNetworkDeviceIdSize); }
  bool operator==(const NetworkDeviceId& other) const { return Compare(other) == 0; }
  bool operator!=(const NetworkDeviceId& other) const { return Compare(other) != 0; }
  bool operator<(const NetworkDeviceId& other) const { return Compare(other) < 0; }
  bool operator<=(const NetworkDeviceId& other) const { return Compare(other) <= 0; }
  bool operator>(const NetworkDeviceId& other) const { return Compare(other) > 0; }
  bool operator>=(const NetworkDeviceId& other) const { return Compare(other) >= 0; }
  std::string ToString() const {
    char result[2 * 6 + 5 + 1] = {};
    snprintf(result, sizeof(result), DEVICE_ID_FMT, DEVICE_ID_HEX(*this));
    return result;
  }
  const uint8_t* data() const { return &data_[0]; }
  uint8_t* data() { return &data_[0]; }

  NetworkDeviceId PlusOne() const;

  static constexpr size_t size() { return kNetworkDeviceIdSize; }

 private:
  static inline constexpr size_t kNetworkDeviceIdSize = 6;
  uint8_t data_[kNetworkDeviceIdSize];
};

struct ProtocolMessage {
  NetworkDeviceId sender = NetworkDeviceId();
  NetworkDeviceId originator = NetworkDeviceId();
  Precedence precedence = 0;
  PatternBits currentPattern = 0;
  PatternBits nextPattern = 0;
  NumHops numHops = 0;
  // Times are sent over the wire as milliseconds since that event, but kept as microseconds internally.
  Microseconds currentPatternStartTime = 0;
  Microseconds lastOriginationTime = 0;
  // Receipt values are not sent over the wire.
  // Note that, when sending, `receiptNetworkId` and `receiptNetworkType` represent
  // the network where our followed next hop is; or 0 / kLeading if we are leading.
  NetworkId receiptNetworkId = 0;
  NetworkType receiptNetworkType = NetworkType::kLeading;
  std::string receiptDetails;

#if JL_IS_CONFIG(CREATURE)
  int receiptRssi = -1000;
  OptionalMicroseconds receiptTime;
  uint32_t creatureColor = 0;
  bool isCreature = false;
  bool isPartying = false;
#endif  // CREATURE

  std::optional<OrrerySceneId> orrerySceneId;

  bool IsEqualExceptOriginationTime(const ProtocolMessage& other) const {
    return sender == other.sender && originator == other.originator && precedence == other.precedence &&
           currentPattern == other.currentPattern && nextPattern == other.nextPattern && numHops == other.numHops &&
           currentPatternStartTime == other.currentPatternStartTime && receiptNetworkId == other.receiptNetworkId &&
           receiptNetworkType == other.receiptNetworkType;
  }
  bool operator==(const ProtocolMessage& other) const {
    return IsEqualExceptOriginationTime(other) && lastOriginationTime == other.lastOriginationTime;
  }
  bool operator!=(const ProtocolMessage& other) const { return !(*this == other); }
};

// Interpretation of ProtocolMessage::orrerySceneId.
enum class OrreryScene : uint8_t {
  kPaused = 0,
  kRealistic = 1,
  kAlign = 2,
  kSilly = 3,
  kFocusMercury = 4,
  kFocusVenus = 5,
  kFocusEarth = 6,
  kFocusMars = 7,
  kFocusJupiter = 8,
  kFocusSaturn = 9,
  kFocusUranus = 10,
  kFocusNeptune = 11,
  kFocusSun = 12,
  kMercuryRetrograde = 13,

  kInvalidScene = 100,
  kMinScene = kPaused,
  kMaxScene = kMercuryRetrograde,
};

const char* OrrerySceneToString(OrreryScene scene);

std::string DisplayBitsAsBinary(PatternBits p);
std::string NetworkMessageToString(const ProtocolMessage& message);

}  // namespace jazzlights

#endif  // JL_PROTOCOL_WIRE_TYPES_H
