#include "jazzlights/orrery_common.h"

#include <cinttypes>

#include "jazzlights/network/network.h"

namespace jazzlights {

const char* OrrerySceneToString(OrreryScene scene) {
  switch (scene) {
    case OrreryScene::Paused: return "Paused";
    case OrreryScene::Realistic: return "Realistic";
    case OrreryScene::Align: return "Align";
    case OrreryScene::Silly: return "Silly";
    case OrreryScene::FocusMercury: return "FocusMercury";
    case OrreryScene::FocusVenus: return "FocusVenus";
    case OrreryScene::FocusEarth: return "FocusEarth";
    case OrreryScene::FocusMars: return "FocusMars";
    case OrreryScene::FocusJupiter: return "FocusJupiter";
    case OrreryScene::FocusSaturn: return "FocusSaturn";
    case OrreryScene::FocusUranus: return "FocusUranus";
    case OrreryScene::FocusNeptune: return "FocusNeptune";
    case OrreryScene::FocusSun: return "FocusSun";
    case OrreryScene::MercuryRetrograde: return "MercuryRetrograde";
    case OrreryScene::kInvalidScene: return "Invalid";
  }
  return "Unknown";
}

#if JL_IS_CONFIG(ORRERY_PLANET) || JL_IS_CONFIG(ORRERY_LEADER) || JL_IS_CONTROLLER(CORE2AWS) || \
    JL_IS_CONTROLLER(CORES3) || PIO_UNIT_TESTING

namespace {
constexpr uint8_t kOrreryFlagSpeed = 0x01;
constexpr uint8_t kOrreryFlagPosition = 0x02;
constexpr uint8_t kOrreryFlagCalibration = 0x04;
constexpr uint8_t kOrreryFlagLedPattern = 0x08;
constexpr uint8_t kOrreryFlagLedBrightness = 0x10;
constexpr uint8_t kOrreryFlagLedBasePrecedence = 0x20;
constexpr uint8_t kOrreryFlagTimeHallSensorLastOpened = 0x40;
constexpr uint8_t kOrreryFlagLedPrecedenceGain = 0x80;

constexpr uint8_t kOrreryFlag2TimeHallSensorLastClosed = 0x01;
constexpr uint8_t kOrreryFlag2LastOpenDuration = 0x02;
constexpr uint8_t kOrreryFlag2LastClosedDuration = 0x04;
}  // namespace

bool WriteOrreryMessage(const OrreryMessage& msg, NetworkWriter& writer) {
  if (!writer.WriteUint8(static_cast<uint8_t>(msg.type))) { return false; }
  uint8_t flags = 0;
  if (msg.speed) { flags |= kOrreryFlagSpeed; }
  if (msg.position) { flags |= kOrreryFlagPosition; }
  if (msg.calibration) { flags |= kOrreryFlagCalibration; }
  if (msg.timeHallSensorLastOpened && !TimeSinceMs32Overflows(*msg.timeHallSensorLastOpened)) {
    flags |= kOrreryFlagTimeHallSensorLastOpened;
  }
  if (msg.ledPattern) { flags |= kOrreryFlagLedPattern; }
  if (msg.ledBrightness) { flags |= kOrreryFlagLedBrightness; }
  if (msg.ledBasePrecedence) { flags |= kOrreryFlagLedBasePrecedence; }
  if (msg.ledPrecedenceGain) { flags |= kOrreryFlagLedPrecedenceGain; }
  if (!writer.WriteUint8(flags)) { return false; }

  uint8_t flags2 = 0;
  if (msg.timeHallSensorLastClosed && !TimeSinceMs32Overflows(*msg.timeHallSensorLastClosed)) {
    flags2 |= kOrreryFlag2TimeHallSensorLastClosed;
  }
  if (msg.lastOpenDuration && !DurationMs32Overflows(*msg.lastOpenDuration)) { flags2 |= kOrreryFlag2LastOpenDuration; }
  if (msg.lastClosedDuration && !DurationMs32Overflows(*msg.lastClosedDuration)) {
    flags2 |= kOrreryFlag2LastClosedDuration;
  }
  if (!writer.WriteUint8(flags2)) { return false; }

  if (!writer.WriteUint32(msg.leaderBootId)) { return false; }
  if (!writer.WriteUint32(msg.leaderSequenceNumber)) { return false; }
  if (msg.speed && !writer.WriteInt32(*msg.speed)) { return false; }
  if (msg.position && !writer.WriteUint32(*msg.position)) { return false; }
  if (msg.calibration && !writer.WriteUint32(*msg.calibration)) { return false; }
  if (!writer.WriteOptionalTimeSinceMs32(msg.timeHallSensorLastOpened)) { return false; }
  if (!writer.WriteOptionalTimeSinceMs32(msg.timeHallSensorLastClosed)) { return false; }
  if (!writer.WriteOptionalDurationMs32(msg.lastOpenDuration)) { return false; }
  if (!writer.WriteOptionalDurationMs32(msg.lastClosedDuration)) { return false; }
  if (msg.ledPattern && !writer.WriteUint32(static_cast<uint32_t>(*msg.ledPattern))) { return false; }
  if (msg.ledBrightness && !writer.WriteUint8(*msg.ledBrightness)) { return false; }
  if (msg.ledBasePrecedence && !writer.WriteUint16(*msg.ledBasePrecedence)) { return false; }
  if (msg.ledPrecedenceGain && !writer.WriteUint16(*msg.ledPrecedenceGain)) { return false; }
  return true;
}

bool ReadOrreryMessage(NetworkReader& reader, OrreryMessage* msg) {
  uint8_t typeByte;
  if (!reader.ReadUint8(&typeByte)) { return false; }
  msg->type = static_cast<OrreryMessageType>(typeByte);
  uint8_t flags;
  if (!reader.ReadUint8(&flags)) { return false; }
  uint8_t flags2;
  if (!reader.ReadUint8(&flags2)) { return false; }

  if (!reader.ReadUint32(&msg->leaderBootId)) { return false; }
  if (!reader.ReadUint32(&msg->leaderSequenceNumber)) { return false; }
  if (flags & kOrreryFlagSpeed) {
    int32_t speed;
    if (!reader.ReadInt32(&speed)) { return false; }
    msg->speed = speed;
  } else {
    msg->speed = std::nullopt;
  }
  if (flags & kOrreryFlagPosition) {
    uint32_t position;
    if (!reader.ReadUint32(&position)) { return false; }
    msg->position = position;
  } else {
    msg->position = std::nullopt;
  }
  if (flags & kOrreryFlagCalibration) {
    uint32_t calibration;
    if (!reader.ReadUint32(&calibration)) { return false; }
    msg->calibration = calibration;
  } else {
    msg->calibration = std::nullopt;
  }
  if (flags & kOrreryFlagTimeHallSensorLastOpened) {
    Microseconds t;
    if (!reader.ReadTimeSinceMs32(&t)) { return false; }
    msg->timeHallSensorLastOpened = t;
  } else {
    msg->timeHallSensorLastOpened = std::nullopt;
  }
  if (flags2 & kOrreryFlag2TimeHallSensorLastClosed) {
    Microseconds t;
    if (!reader.ReadTimeSinceMs32(&t)) { return false; }
    msg->timeHallSensorLastClosed = t;
  } else {
    msg->timeHallSensorLastClosed = std::nullopt;
  }
  if (flags2 & kOrreryFlag2LastOpenDuration) {
    uint32_t lastOpenDurationMs;
    if (!reader.ReadUint32(&lastOpenDurationMs)) { return false; }
    msg->lastOpenDuration = MillisecondsToMicroseconds(static_cast<Milliseconds>(lastOpenDurationMs));
  } else {
    msg->lastOpenDuration = std::nullopt;
  }
  if (flags2 & kOrreryFlag2LastClosedDuration) {
    uint32_t lastClosedDurationMs;
    if (!reader.ReadUint32(&lastClosedDurationMs)) { return false; }
    msg->lastClosedDuration = MillisecondsToMicroseconds(static_cast<Milliseconds>(lastClosedDurationMs));
  } else {
    msg->lastClosedDuration = std::nullopt;
  }
  if (flags & kOrreryFlagLedPattern) {
    PatternBits ledPattern;
    if (!reader.ReadPatternBits(&ledPattern)) { return false; }
    msg->ledPattern = ledPattern;
  } else {
    msg->ledPattern = std::nullopt;
  }
  if (flags & kOrreryFlagLedBrightness) {
    uint8_t ledBrightness;
    if (!reader.ReadUint8(&ledBrightness)) { return false; }
    msg->ledBrightness = ledBrightness;
  } else {
    msg->ledBrightness = std::nullopt;
  }
  if (flags & kOrreryFlagLedBasePrecedence) {
    Precedence ledBasePrecedence;
    if (!reader.ReadUint16(&ledBasePrecedence)) { return false; }
    msg->ledBasePrecedence = ledBasePrecedence;
  } else {
    msg->ledBasePrecedence = std::nullopt;
  }
  if (flags & kOrreryFlagLedPrecedenceGain) {
    Precedence ledPrecedenceGain;
    if (!reader.ReadUint16(&ledPrecedenceGain)) { return false; }
    msg->ledPrecedenceGain = ledPrecedenceGain;
  } else {
    msg->ledPrecedenceGain = std::nullopt;
  }
  return true;
}

std::string OrreryMessageToString(const OrreryMessage& msg) {
  char buf[512];
  int n = snprintf(buf, sizeof(buf), "%s seq=%" PRIu32 " boot=%08" PRIx32,
                   msg.type == OrreryMessageType::LeaderCommand ? "LeaderCommand" : "FollowerResponse",
                   static_cast<uint32_t>(msg.leaderSequenceNumber), static_cast<uint32_t>(msg.leaderBootId));
  if (msg.speed) { n += snprintf(buf + n, sizeof(buf) - n, " speed=%" PRId32, static_cast<int32_t>(*msg.speed)); }
  if (msg.position) { n += snprintf(buf + n, sizeof(buf) - n, " pos=%" PRIu32, static_cast<uint32_t>(*msg.position)); }
  if (msg.calibration) {
    n += snprintf(buf + n, sizeof(buf) - n, " cal=%" PRIu32, static_cast<uint32_t>(*msg.calibration));
  }
  if (msg.timeHallSensorLastOpened) {
    n += snprintf(buf + n, sizeof(buf) - n, " lastOpen=%" PRId64 "s_ago",
                  static_cast<int64_t>((timeMicros() - *msg.timeHallSensorLastOpened) / kMicrosecondsPerSecond));
  }
  if (msg.timeHallSensorLastClosed) {
    n += snprintf(buf + n, sizeof(buf) - n, " lastClosed=%" PRId64 "s_ago",
                  static_cast<int64_t>((timeMicros() - *msg.timeHallSensorLastClosed) / kMicrosecondsPerSecond));
  }
  if (msg.lastOpenDuration) {
    n += snprintf(buf + n, sizeof(buf) - n, " openDur=%" PRId64 "ms",
                  static_cast<int64_t>(*msg.lastOpenDuration / kMicrosecondsPerMillisecond));
  }
  if (msg.lastClosedDuration) {
    n += snprintf(buf + n, sizeof(buf) - n, " closedDur=%" PRId64 "ms",
                  static_cast<int64_t>(*msg.lastClosedDuration / kMicrosecondsPerMillisecond));
  }
  if (msg.ledPattern) {
    n += snprintf(buf + n, sizeof(buf) - n, " pattern=%08" PRIx32, static_cast<uint32_t>(*msg.ledPattern));
  }
  if (msg.ledBrightness) {
    n += snprintf(buf + n, sizeof(buf) - n, " brightness=%u", static_cast<unsigned int>(*msg.ledBrightness));
  }
  if (msg.ledBasePrecedence) {
    n += snprintf(buf + n, sizeof(buf) - n, " basePrec=%u", static_cast<unsigned int>(*msg.ledBasePrecedence));
  }
  if (msg.ledPrecedenceGain) {
    n += snprintf(buf + n, sizeof(buf) - n, " precGain=%u", static_cast<unsigned int>(*msg.ledPrecedenceGain));
  }
  return std::string(buf);
}

const char* GetPlanetName(Planet planet) {
  switch (planet) {
    case Planet::Mercury: return "Mercury";
    case Planet::Venus: return "Venus";
    case Planet::Earth: return "Earth";
    case Planet::Mars: return "Mars";
    case Planet::Jupiter: return "Jupiter";
    case Planet::Saturn: return "Saturn";
    case Planet::Uranus: return "Uranus";
    case Planet::Neptune: return "Neptune";
    case Planet::Sun: return "Sun";
    case Planet::Pluto: return "Pluto";
    case Planet::All: return "Global";
  }
  return "Unknown";
}

#endif  // JL_IS_CONFIG(ORRERY_PLANET) || JL_IS_CONFIG(ORRERY_LEADER) || JL_IS_CONTROLLER(CORE2AWS) ||
        // JL_IS_CONTROLLER(CORES3) || PIO_UNIT_TESTING

}  // namespace jazzlights
