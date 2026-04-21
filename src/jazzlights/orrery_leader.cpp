#include "jazzlights/orrery_leader.h"

#if JL_IS_CONFIG(ORRERY_LEADER)

#include <cinttypes>
#include <cstdio>
#include <cstring>

#include "jazzlights/motor.h"
#include "jazzlights/network/max485_bus.h"
#include "jazzlights/network/network.h"
#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/log.h"

namespace jazzlights {

namespace {
#if JL_IS_CONTROLLER(ATOM_MATRIX) || JL_IS_CONTROLLER(ATOM_LITE)
constexpr int kMax485TxPin = 26;
constexpr int kMax485RxPin = 32;
#elif JL_IS_CONTROLLER(ATOM_S3) || JL_IS_CONTROLLER(ATOM_S3_LITE)
constexpr int kMax485TxPin = 2;
constexpr int kMax485RxPin = 1;
#elif JL_IS_CONTROLLER(M5STAMP_S3)
constexpr int kMax485TxPin = 43;
constexpr int kMax485RxPin = 44;
#elif JL_IS_CONTROLLER(CORE2AWS)
constexpr int kMax485TxPin = 2;
constexpr int kMax485RxPin = 33;
#else
// For CoreS3 and CoreS3-SE:
constexpr int kMax485TxPin = 13;
constexpr int kMax485RxPin = 1;
#error "unsupported controller for Max485BusHandler"
#endif
}  // namespace

OrreryLeader* OrreryLeader::Get() {
  static OrreryLeader sOrreryLeader;
  return &sOrreryLeader;
}

OrreryLeader::OrreryLeader()
    : bootId_(UnpredictableRandom::Get32bits()), max485BusLeader_(UART_NUM_2, kMax485TxPin, kMax485RxPin) {
  for (int i = 0; i < kNumPlanets; i++) {
    Planet planet = static_cast<Planet>(static_cast<int>(Planet::Mercury) + i);
    OrreryMessage& msg = messages_[planet];
    msg.type = OrreryMessageType::LeaderCommand;
    msg.leaderBootId = bootId_;
    msg.leaderSequenceNumber = nextSequenceNumber_++;
    msg.speed = 0;
    msg.ledBrightness = kDefaultPlanetBrightness;
    msg.ledPattern = kPlanetPattern;
    msg.ledBasePrecedence = kDefaultPlanetBasePrecedence;
    msg.ledPrecedenceGain = kDefaultPlanetPrecedenceGain;
    SendMessage(planet);
  }
}

void OrreryLeader::SetSpeed(Planet planet, int32_t speed) {
  if (planet == Planet::All) {
    for (int i = 0; i < kNumPlanets; i++) {
      SetSpeed(static_cast<Planet>(static_cast<int>(Planet::Mercury) + i), speed);
    }
    return;
  }
  auto it = messages_.find(planet);
  if (it != messages_.end()) {
    OrreryMessage& msg = it->second;
    if (!msg.speed.has_value() || *msg.speed != speed) {
      msg.speed = speed;
      jll_info("OrreryLeader setting planet %s milli-RPM to %" PRId32, GetPlanetName(planet), speed);
      SendMessage(planet);
    }
  }
}

int32_t OrreryLeader::GetSpeed(Planet planet) const {
  if (planet == Planet::All) { planet = Planet::Mercury; }
  auto it = messages_.find(planet);
  if (it != messages_.end() && it->second.speed.has_value()) { return *it->second.speed; }
  return 0;
}

void OrreryLeader::SetPosition(Planet planet, std::optional<uint32_t> position) {
  if (planet == Planet::All) {
    for (int i = 0; i < kNumPlanets; i++) {
      SetPosition(static_cast<Planet>(static_cast<int>(Planet::Mercury) + i), position);
    }
    return;
  }
  auto it = messages_.find(planet);
  if (it != messages_.end()) {
    OrreryMessage& msg = it->second;
    if (msg.position != position) {
      msg.position = position;
      if (position.has_value()) {
        jll_info("OrreryLeader setting planet %s position to %" PRIu32, GetPlanetName(planet), *position);
      } else {
        jll_info("OrreryLeader unsetting planet %s position", GetPlanetName(planet));
      }
      SendMessage(planet);
    }
  }
}

std::optional<uint32_t> OrreryLeader::GetPosition(Planet planet) const {
  if (planet == Planet::All) { planet = Planet::Mercury; }
  auto it = responses_.find(planet);
  if (it != responses_.end()) { return it->second.position; }
  return std::nullopt;
}

std::optional<uint32_t> OrreryLeader::GetCalibration(Planet planet) const {
  if (planet == Planet::All) { planet = Planet::Mercury; }
  auto it = responses_.find(planet);
  if (it != responses_.end()) { return it->second.calibration; }
  return std::nullopt;
}

void OrreryLeader::SetBrightness(Planet planet, uint8_t brightness) {
  if (planet == Planet::All) {
    for (int i = 0; i < kNumPlanets; i++) {
      SetBrightness(static_cast<Planet>(static_cast<int>(Planet::Mercury) + i), brightness);
    }
    return;
  }
  auto it = messages_.find(planet);
  if (it != messages_.end()) {
    OrreryMessage& msg = it->second;
    if (!msg.ledBrightness.has_value() || *msg.ledBrightness != brightness) {
      msg.ledBrightness = brightness;
      jll_info("OrreryLeader setting planet %s brightness to %u", GetPlanetName(planet), brightness);
      SendMessage(planet);
    }
  }
}

uint8_t OrreryLeader::GetBrightness(Planet planet) const {
  if (planet == Planet::All) { planet = Planet::Mercury; }
  auto it = messages_.find(planet);
  if (it != messages_.end() && it->second.ledBrightness.has_value()) { return *it->second.ledBrightness; }
  return kDefaultPlanetBrightness;
}

void OrreryLeader::SetLedPattern(Planet planet, uint32_t ledPattern) {
  if (planet == Planet::All) {
    for (int i = 0; i < kNumPlanets; i++) {
      SetLedPattern(static_cast<Planet>(static_cast<int>(Planet::Mercury) + i), ledPattern);
    }
    return;
  }
  auto it = messages_.find(planet);
  if (it != messages_.end()) {
    OrreryMessage& msg = it->second;
    if (!msg.ledPattern.has_value() || *msg.ledPattern != ledPattern) {
      msg.ledPattern = ledPattern;
      jll_info("OrreryLeader setting planet %s LED pattern to 0x%08" PRIx32, GetPlanetName(planet), ledPattern);
      SendMessage(planet);
    }
  }
}

uint32_t OrreryLeader::GetLedPattern(Planet planet) const {
  if (planet == Planet::All) { planet = Planet::Mercury; }
  auto it = messages_.find(planet);
  if (it != messages_.end() && it->second.ledPattern.has_value()) { return *it->second.ledPattern; }
  return kPlanetPattern;
}

std::optional<Milliseconds> OrreryLeader::GetTimeHallSensorLastOpened(Planet planet) const {
  auto it = responses_.find(planet);
  if (it != responses_.end()) { return it->second.timeHallSensorLastOpened; }
  return std::nullopt;
}

std::optional<Milliseconds> OrreryLeader::GetTimeHallSensorLastClosed(Planet planet) const {
  auto it = responses_.find(planet);
  if (it != responses_.end()) { return it->second.timeHallSensorLastClosed; }
  return std::nullopt;
}

std::optional<Milliseconds> OrreryLeader::GetLastOpenDuration(Planet planet) const {
  auto it = responses_.find(planet);
  if (it != responses_.end()) { return it->second.lastOpenDuration; }
  return std::nullopt;
}

std::optional<Milliseconds> OrreryLeader::GetLastClosedDuration(Planet planet) const {
  auto it = responses_.find(planet);
  if (it != responses_.end()) { return it->second.lastClosedDuration; }
  return std::nullopt;
}

std::optional<Milliseconds> OrreryLeader::GetLastHeardTime(Planet planet) const {
  auto it = lastHeardTime_.find(planet);
  if (it != lastHeardTime_.end()) { return it->second; }
  return std::nullopt;
}

void OrreryLeader::SendMessage(Planet planet) {
  auto it = messages_.find(planet);
  if (it != messages_.end()) {
    OrreryMessage& msg = it->second;
    msg.leaderSequenceNumber = nextSequenceNumber_++;
    max485BusLeader_.SetMessageToSend(static_cast<BusId>(planet), msg);
  }
}

void OrreryLeader::RunLoop(Milliseconds currentTime) {
  BusId destBusId, srcBusId;
  OrreryMessage msg;
  while (max485BusLeader_.ReadMessage(&msg, &destBusId, &srcBusId)) {
    if (msg.type == OrreryMessageType::FollowerResponse) {
      Planet planet = static_cast<Planet>(srcBusId);
      responses_[planet] = msg;
      lastHeardTime_[planet] = currentTime;
      if (msg.calibration.has_value()) { messages_[planet].calibration = msg.calibration; }
    }
  }
}

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_LEADER)
