#include "jazzlights/orrery_common.h"

#if JL_IS_CONFIG(ORRERY_PLANET) || JL_IS_CONFIG(ORRERY_LEADER) || PIO_UNIT_TESTING

#include "jazzlights/network/network.h"

namespace jazzlights {

namespace {
constexpr uint8_t kOrreryFlagSpeed = 0x01;
constexpr uint8_t kOrreryFlagPosition = 0x02;
constexpr uint8_t kOrreryFlagCalibration = 0x04;
constexpr uint8_t kOrreryFlagLedPattern = 0x08;
constexpr uint8_t kOrreryFlagLedBrightness = 0x10;
constexpr uint8_t kOrreryFlagLedPrecedence = 0x20;
constexpr uint8_t kOrreryFlagTimeHallSensorLastOpened = 0x40;
}  // namespace

bool WriteOrreryMessage(const OrreryMessage& msg, NetworkWriter& writer) {
  if (!writer.WriteUint8(static_cast<uint8_t>(msg.type))) { return false; }
  uint8_t flags = 0;
  if (msg.speed.has_value()) { flags |= kOrreryFlagSpeed; }
  if (msg.position.has_value()) { flags |= kOrreryFlagPosition; }
  if (msg.calibration.has_value()) { flags |= kOrreryFlagCalibration; }
  if (msg.timeHallSensorLastOpened.has_value()) { flags |= kOrreryFlagTimeHallSensorLastOpened; }
  if (msg.ledPattern.has_value()) { flags |= kOrreryFlagLedPattern; }
  if (msg.ledBrightness.has_value()) { flags |= kOrreryFlagLedBrightness; }
  if (msg.ledPrecedence.has_value()) { flags |= kOrreryFlagLedPrecedence; }
  if (!writer.WriteUint8(flags)) { return false; }
  if (!writer.WriteUint32(msg.leaderBootId)) { return false; }
  if (!writer.WriteUint32(msg.leaderSequenceNumber)) { return false; }
  if (msg.speed.has_value() && !writer.WriteInt32(*msg.speed)) { return false; }
  if (msg.position.has_value() && !writer.WriteUint32(*msg.position)) { return false; }
  if (msg.calibration.has_value() && !writer.WriteUint32(*msg.calibration)) { return false; }
  if (msg.timeHallSensorLastOpened.has_value() && !writer.WriteUint32(timeMillis() - *msg.timeHallSensorLastOpened)) {
    return false;
  }
  if (msg.ledPattern.has_value() && !writer.WriteUint32(static_cast<uint32_t>(*msg.ledPattern))) { return false; }
  if (msg.ledBrightness.has_value() && !writer.WriteUint8(*msg.ledBrightness)) { return false; }
  if (msg.ledPrecedence.has_value() && !writer.WriteUint16(*msg.ledPrecedence)) { return false; }
  return true;
}

bool ReadOrreryMessage(NetworkReader& reader, OrreryMessage* msg) {
  uint8_t typeByte;
  if (!reader.ReadUint8(&typeByte)) { return false; }
  msg->type = static_cast<OrreryMessageType>(typeByte);
  uint8_t flags;
  if (!reader.ReadUint8(&flags)) { return false; }
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
    uint32_t delta;
    if (!reader.ReadUint32(&delta)) { return false; }
    msg->timeHallSensorLastOpened = timeMillis() - delta;
  } else {
    msg->timeHallSensorLastOpened = std::nullopt;
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
  if (flags & kOrreryFlagLedPrecedence) {
    Precedence ledPrecedence;
    if (!reader.ReadUint16(&ledPrecedence)) { return false; }
    msg->ledPrecedence = ledPrecedence;
  } else {
    msg->ledPrecedence = std::nullopt;
  }
  return true;
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
  }
  return "Unknown";
}

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_PLANET) || JL_IS_CONFIG(ORRERY_LEADER)
