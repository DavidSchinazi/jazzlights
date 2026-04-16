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
  }
}

void OrreryLeader::SetSpeed(Planet planet, int32_t speed) {
  auto it = messages_.find(planet);
  if (it != messages_.end()) {
    OrreryMessage& msg = it->second;
    if (!msg.speed.has_value() || *msg.speed != speed) {
      msg.speed = speed;
      jll_info("OrreryLeader setting planet %s speed to %" PRId32, GetPlanetName(planet), speed);
      SendMessage(planet);
    }
  }
}

int32_t OrreryLeader::GetSpeed(Planet planet) const {
  auto it = messages_.find(planet);
  if (it != messages_.end() && it->second.speed.has_value()) { return *it->second.speed; }
  return 0;
}

void OrreryLeader::SetBrightness(Planet planet, uint8_t brightness) {
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
  auto it = messages_.find(planet);
  if (it != messages_.end() && it->second.ledBrightness.has_value()) { return *it->second.ledBrightness; }
  return kDefaultPlanetBrightness;
}

std::optional<uint32_t> OrreryLeader::GetTimeHallSensorLastOpened(Planet planet) const {
  auto it = responses_.find(planet);
  if (it != responses_.end()) { return it->second.timeHallSensorLastOpened; }
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

void OrreryLeader::RunLoop(Milliseconds /*currentTime*/) {
  BusId destBusId, srcBusId;
  OrreryMessage msg;
  if (!max485BusLeader_.ReadMessage(&msg, &destBusId, &srcBusId)) { return; }

  if (msg.type == OrreryMessageType::FollowerResponse) {
    responses_[static_cast<Planet>(srcBusId)] = msg;
    if (msg.speed.has_value()) {
      jll_info("OrreryLeader received response for planet %u: %" PRId32,
               static_cast<uint8_t>(srcBusId) - static_cast<uint8_t>(Planet::Mercury), *msg.speed);
    }
  }
}

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_LEADER)
