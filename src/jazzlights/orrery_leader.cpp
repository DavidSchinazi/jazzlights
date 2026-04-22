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

constexpr int kSwitch1Pin = kPinB2;
constexpr int kSwitch3Pin = kPinC2;
constexpr int kSwitch4Pin = kPinC1;

}  // namespace

const char* OrrerySceneToString(OrreryScene scene) {
  switch (scene) {
    case OrreryScene::Paused: return "Paused";
    case OrreryScene::Realistic: return "Realistic";
    case OrreryScene::Align: return "Align";
  }
  return "Unknown";
}

OrreryLeader* OrreryLeader::Get() {
  static OrreryLeader sOrreryLeader;
  return &sOrreryLeader;
}

OrreryLeader::OrreryLeader()
    : bootId_(UnpredictableRandom::Get32bits()),
      max485BusLeader_(UART_NUM_2, kMax485TxPin, kMax485RxPin),
      switch1_(kSwitch1Pin, *this),
      switch3_(kSwitch3Pin, *this),
      switch4_(kSwitch4Pin, *this),
      scene_(OrreryScene::Paused) {
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
  HandleSwitch1(switch1_.IsClosed());
  HandleSwitch4(switch4_.IsClosed());
}

void OrreryLeader::SetScene(OrreryScene scene) {
  scene_ = scene;
  jll_info("OrreryLeader setting scene to %s", OrrerySceneToString(scene));
  if (scene == OrreryScene::Paused) {
    SetSpeed(Planet::All, 0);
  } else if (scene == OrreryScene::Realistic) {
    // These values are based on the real speeds of the planets with a log scale applied.
    // RPM = 670.163 * log10(realSpeed) + 5419.638
    SetSpeed(Planet::Mercury, 2000);
    SetSpeed(Planet::Venus, 1723);
    SetSpeed(Planet::Earth, 1580);
    SetSpeed(Planet::Mars, 1393);
    SetSpeed(Planet::Jupiter, 849);
    SetSpeed(Planet::Saturn, 580);
    SetSpeed(Planet::Uranus, 270);
    SetSpeed(Planet::Neptune, 100);
  } else if (scene == OrreryScene::Align) {
    waitingForAlignment_ = true;
    SetPosition(Planet::All, 0);
    for (int i = 0; i < kNumPlanetsWithoutSun; i++) {
      Planet planet = static_cast<Planet>(static_cast<int>(Planet::Mercury) + i);
      int32_t speed = GetSpeed(planet);
      if (0 <= speed && speed < 1000) {
        SetSpeed(planet, 1000);
      } else if (-1000 < speed && speed <= 0) {
        SetSpeed(planet, -1000);
      }
    }
  }
}

void OrreryLeader::SetSpeed(Planet planet, int32_t speed) {
  if (planet == Planet::All) {
    for (int i = 0; i < kNumPlanetsWithoutSun; i++) {
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
    for (int i = 0; i < kNumPlanetsWithoutSun; i++) {
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

std::optional<Milliseconds> OrreryLeader::GetMaxRtt(Planet planet) const {
  auto it = maxRtt_.find(planet);
  if (it != maxRtt_.end()) { return it->second; }
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
  switch1_.RunLoop();
  switch3_.RunLoop();
  switch4_.RunLoop();
  BusId destBusId, srcBusId;
  OrreryMessage msg;
  Milliseconds rtt;
  while (max485BusLeader_.ReadMessage(&msg, &destBusId, &srcBusId, &rtt)) {
    if (msg.type == OrreryMessageType::FollowerResponse) {
      Planet planet = static_cast<Planet>(srcBusId);
      responses_[planet] = msg;
      lastHeardTime_[planet] = currentTime;
      if (rtt >= 0) {
        if (maxRtt_.find(planet) == maxRtt_.end() || rtt > maxRtt_[planet]) { maxRtt_[planet] = rtt; }
      }
      if (msg.calibration.has_value()) { messages_[planet].calibration = msg.calibration; }
    }
  }

  if (scene_ == OrreryScene::Align && waitingForAlignment_) {
    bool allArrived = true;
    for (int i = 0; i < kNumPlanetsWithoutSun; i++) {
      Planet planet = static_cast<Planet>(static_cast<int>(Planet::Mercury) + i);
      auto itHeard = lastHeardTime_.find(planet);
      if (itHeard == lastHeardTime_.end() || currentTime - itHeard->second > 10000) {
        // Consider arrived if we haven't heard from it for 10s.
        continue;
      }
      auto it = responses_.find(planet);
      if (it == responses_.end()) {
        // Consider arrived if we've never heard from it.
        continue;
      }
      const OrreryMessage& resp = it->second;
      bool atZero = false;
      if (resp.position.has_value()) {
        uint32_t pos = *resp.position;
        if (pos < 4000 || pos > 356000) { atZero = true; }
      }
      if (!atZero || !resp.speed.has_value() || *resp.speed > 10) {
        allArrived = false;
        break;
      }
    }
    if (allArrived) {
      jll_info("%u Planets have aligned", timeMillis());
      waitingForAlignment_ = false;
      SetPosition(Planet::All, std::nullopt);
      SetSpeed(Planet::All, 1000);
    }
  }
}

void OrreryLeader::StateChanged(uint8_t pin, bool isClosed) {
  jll_info("%u switch on pin %u is now %s", timeMillis(), pin, (isClosed ? "closed" : "open"));
  if (pin == kSwitch1Pin) {
    HandleSwitch1(isClosed);
  } else if (pin == kSwitch4Pin) {
    HandleSwitch4(isClosed);
  }
}

void OrreryLeader::HandleSwitch1(bool isClosed) { SetScene(isClosed ? OrreryScene::Paused : OrreryScene::Realistic); }

void OrreryLeader::HandleSwitch4(bool isClosed) {
  for (int i = 0; i < kNumPlanets; i++) {
    Planet planet = static_cast<Planet>(static_cast<int>(Planet::Mercury) + i);
    uint32_t pattern = GetLedPattern(planet);
    if (isClosed) {
      pattern |= kPlanetPatternHallSensorBit;
    } else {
      pattern &= ~kPlanetPatternHallSensorBit;
    }
    SetLedPattern(planet, pattern);
  }
}

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_LEADER)
