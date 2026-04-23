#include "jazzlights/orrery_leader.h"

#if JL_IS_CONFIG(ORRERY_LEADER)

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <vector>

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
  }
  return "Unknown";
}

OrreryLeader* OrreryLeader::Get() {
  static OrreryLeader sOrreryLeader;
  return &sOrreryLeader;
}

void OrreryLeader::Setup(Player& player) {
  player_ = &player;
  player_->SetOverriddenPatternWatcher(this);
}

void OrreryLeader::OnOverriddenPattern(std::optional<PatternBits> pattern) {
  if (pattern == patternOverride_) { return; }
  patternOverride_ = pattern;
  if (patternOverride_.has_value()) {
    jll_info("%u OrreryLeader overriding pattern with 0x%08x", timeMillis(), *patternOverride_);
    for (int i = 0; i < kNumPlanets; i++) {
      Planet planet = static_cast<Planet>(static_cast<int>(Planet::Mercury) + i);
      patternBeforeOverride_[planet] = GetLedPattern(planet);
    }
    SetLedPattern(Planet::All, *patternOverride_);
  } else {
    jll_info("%u OrreryLeader disabling pattern override", timeMillis());
    for (int i = 0; i < kNumPlanets; i++) {
      std::optional<uint32_t> ledPattern;
      Planet planet = static_cast<Planet>(static_cast<int>(Planet::Mercury) + i);
      auto it = patternBeforeOverride_.find(planet);
      if (it != patternBeforeOverride_.end()) { ledPattern = it->second; }
      if (ledPattern.has_value()) {
        SetLedPattern(planet, *ledPattern);
      } else {
        SetLedPattern(planet, kPlanetPattern);
      }
    }
  }
}

OrreryLeader::OrreryLeader()
    : bootId_(UnpredictableRandom::Get32bits()),
      max485BusLeader_(UART_NUM_2, kMax485TxPin, kMax485RxPin),
      switch1_(kSwitch1Pin, *this),
      switch3_(kSwitch3Pin, *this),
      switch4_(kSwitch4Pin, *this),
      scene_(OrreryScene::Paused),
      sceneStartTime_(timeMillis()),
      lastRandomSceneTime_(0),
      nextRandomSceneDuration_(0) {
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
  sceneStartTime_ = timeMillis();
  if (scene == OrreryScene::Realistic) {
    if (!switch3_.IsClosed()) {
      lastRandomSceneTime_ = sceneStartTime_;
      nextRandomSceneDuration_ = (UnpredictableRandom::GetNumberBetween(2 * 60, 10 * 60)) * 1000;
      jll_info("%u OrreryLeader scheduling random scene in %u seconds", sceneStartTime_,
               nextRandomSceneDuration_ / 1000);
    } else {
      nextRandomSceneDuration_ = 0;
    }
  } else {
    nextRandomSceneDuration_ = 0;
  }
  jll_info("%u OrreryLeader setting scene to %s", sceneStartTime_, OrrerySceneToString(scene));
  if (scene == OrreryScene::Paused) {
    SetSpeed(Planet::All, 0);
  } else if (scene == OrreryScene::Realistic || scene == OrreryScene::Silly || scene == OrreryScene::FocusMercury ||
             scene == OrreryScene::FocusVenus || scene == OrreryScene::FocusEarth || scene == OrreryScene::FocusMars ||
             scene == OrreryScene::FocusJupiter || scene == OrreryScene::FocusSaturn ||
             scene == OrreryScene::FocusUranus || scene == OrreryScene::FocusNeptune ||
             scene == OrreryScene::FocusSun || scene == OrreryScene::MercuryRetrograde) {
    // These values are based on the real speeds of the planets with a log scale applied.
    // RPM = 670.163 * log10(realSpeed) + 5419.638
    struct PlanetSpeed {
      Planet planet;
      int32_t speed;
    };
    const PlanetSpeed speeds[] = {
        {Planet::Mercury, 2000},
        {  Planet::Venus, 1723},
        {  Planet::Earth, 1580},
        {   Planet::Mars, 1393},
        {Planet::Jupiter,  849},
        { Planet::Saturn,  580},
        { Planet::Uranus,  270},
        {Planet::Neptune,  100},
    };
    for (size_t i = 0; i < sizeof(speeds) / sizeof(speeds[0]); i++) {
      int32_t speed = speeds[i].speed;
      if (scene == OrreryScene::Silly && (i % 2) == 1) { speed = -speed; }
      if (scene == OrreryScene::MercuryRetrograde && speeds[i].planet == Planet::Mercury) { speed = -speed; }
      SetSpeed(speeds[i].planet, speed);
    }
    if (scene != OrreryScene::Realistic && scene != OrreryScene::Silly && scene != OrreryScene::MercuryRetrograde) {
      Planet focusedPlanet = Planet::All;
      if (scene == OrreryScene::FocusMercury) {
        focusedPlanet = Planet::Mercury;
      } else if (scene == OrreryScene::FocusVenus) {
        focusedPlanet = Planet::Venus;
      } else if (scene == OrreryScene::FocusEarth) {
        focusedPlanet = Planet::Earth;
      } else if (scene == OrreryScene::FocusMars) {
        focusedPlanet = Planet::Mars;
      } else if (scene == OrreryScene::FocusJupiter) {
        focusedPlanet = Planet::Jupiter;
      } else if (scene == OrreryScene::FocusSaturn) {
        focusedPlanet = Planet::Saturn;
      } else if (scene == OrreryScene::FocusUranus) {
        focusedPlanet = Planet::Uranus;
      } else if (scene == OrreryScene::FocusNeptune) {
        focusedPlanet = Planet::Neptune;
      } else if (scene == OrreryScene::FocusSun) {
        focusedPlanet = Planet::Sun;
      }
      for (int i = 0; i < kNumPlanets; i++) {
        Planet p = static_cast<Planet>(static_cast<int>(Planet::Mercury) + i);
        SetBrightness(p, p == focusedPlanet ? kDefaultPlanetBrightness : 0);
      }
    }
  } else if (scene == OrreryScene::Align) {
    waitingForAlignment_ = true;
    hasPassedHalfway_.clear();
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
    jll_info("%u OrreryLeader setting all planets milli-RPM to %" PRId32, timeMillis(), speed);
    OrreryMessage msg;
    msg.speed = speed;
    SendBroadcastMessage(msg);
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
      jll_info("%u OrreryLeader setting planet %s milli-RPM to %" PRId32, timeMillis(), GetPlanetName(planet), speed);
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

std::optional<int32_t> OrreryLeader::GetReportedSpeed(Planet planet) const {
  if (planet == Planet::All) { planet = Planet::Mercury; }
  auto it = responses_.find(planet);
  if (it != responses_.end()) { return it->second.speed; }
  return std::nullopt;
}

void OrreryLeader::SetPosition(Planet planet, std::optional<uint32_t> position) {
  if (planet == Planet::All) {
    if (position.has_value()) {
      jll_info("%u OrreryLeader setting all planets position to %" PRIu32, timeMillis(), *position);
    } else {
      jll_info("%u OrreryLeader unsetting all planets position", timeMillis());
    }
    OrreryMessage msg;
    msg.position = position.has_value() ? *position : kOrreryPositionNone;
    SendBroadcastMessage(msg);
    for (int i = 0; i < kNumPlanetsWithoutSun; i++) {
      SetPosition(static_cast<Planet>(static_cast<int>(Planet::Mercury) + i), position);
    }
    return;
  }
  auto it = messages_.find(planet);
  if (it != messages_.end()) {
    OrreryMessage& msg = it->second;
    std::optional<uint32_t> wirePosition = position.has_value() ? position : kOrreryPositionNone;
    if (msg.position != wirePosition) {
      msg.position = wirePosition;
      if (position.has_value()) {
        jll_info("%u OrreryLeader setting planet %s position to %" PRIu32, timeMillis(), GetPlanetName(planet),
                 *position);
      } else {
        jll_info("%u OrreryLeader unsetting planet %s position", timeMillis(), GetPlanetName(planet));
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

std::optional<uint32_t> OrreryLeader::GetTargetPosition(Planet planet) const {
  if (planet == Planet::All) { planet = Planet::Mercury; }
  auto it = messages_.find(planet);
  if (it != messages_.end() && it->second.position.has_value() && *it->second.position != kOrreryPositionNone) {
    return it->second.position;
  }
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
    jll_info("%u OrreryLeader setting all planets brightness to %u", timeMillis(), brightness);
    OrreryMessage msg;
    msg.ledBrightness = brightness;
    SendBroadcastMessage(msg);
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
      jll_info("%u OrreryLeader setting planet %s brightness to %u", timeMillis(), GetPlanetName(planet), brightness);
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
  if (patternOverride_.has_value()) { ledPattern = *patternOverride_; }
  if (planet == Planet::All) {
    jll_info("%u OrreryLeader setting all planets LED pattern to 0x%08" PRIx32, timeMillis(), ledPattern);
    OrreryMessage msg;
    msg.ledPattern = ledPattern;
    SendBroadcastMessage(msg);
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
      jll_info("%u OrreryLeader setting planet %s LED pattern to 0x%08" PRIx32, timeMillis(), GetPlanetName(planet),
               ledPattern);
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

void OrreryLeader::SendBroadcastMessage(const OrreryMessage& msg) {
  OrreryMessage broadcastMsg = msg;
  broadcastMsg.type = OrreryMessageType::LeaderCommand;
  broadcastMsg.leaderBootId = bootId_;
  broadcastMsg.leaderSequenceNumber = nextSequenceNumber_++;
  max485BusLeader_.SetMessageToSend(Max485BusHandler::kBusIdBroadcast, broadcastMsg);
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

  if (scene_ == OrreryScene::Realistic && nextRandomSceneDuration_ > 0 &&
      currentTime - lastRandomSceneTime_ > nextRandomSceneDuration_) {
    const std::vector<OrreryScene> repertoire = {OrreryScene::Silly, OrreryScene::MercuryRetrograde,
                                                 OrreryScene::Align};
    OrreryScene nextScene = repertoire[UnpredictableRandom::GetNumberBetween(0, repertoire.size() - 1)];
    jll_info("%u Randomly selecting next scene %s", currentTime, OrrerySceneToString(nextScene));
    SetScene(nextScene);
  }

  if (scene_ == OrreryScene::Silly && currentTime - sceneStartTime_ > 5 * 60 * 1000) {
    jll_info("%u Silly scene ending after 5 minutes, starting Realistic scene", currentTime);
    SetScene(OrreryScene::Realistic);
  }

  if ((scene_ == OrreryScene::FocusMercury || scene_ == OrreryScene::FocusVenus || scene_ == OrreryScene::FocusEarth ||
       scene_ == OrreryScene::FocusMars || scene_ == OrreryScene::FocusJupiter || scene_ == OrreryScene::FocusSaturn ||
       scene_ == OrreryScene::FocusUranus || scene_ == OrreryScene::FocusNeptune || scene_ == OrreryScene::FocusSun) &&
      currentTime - sceneStartTime_ > 60 * 1000) {
    jll_info("%u Focus scene ending after 1 minute, starting Realistic scene", currentTime);
    SetBrightness(Planet::All, kDefaultPlanetBrightness);
    SetScene(OrreryScene::Realistic);
  }

  if (scene_ == OrreryScene::MercuryRetrograde && currentTime - sceneStartTime_ > 60 * 1000) {
    jll_info("%u MercuryRetrograde scene ending after 1 minute, starting Realistic scene", currentTime);
    SetScene(OrreryScene::Realistic);
  }

  if (scene_ == OrreryScene::Align) {
    if (waitingForAlignment_) {
      bool allArrived = true;
      for (int i = 0; i < kNumPlanetsWithoutSun; i++) {
        Planet planet = static_cast<Planet>(static_cast<int>(Planet::Mercury) + i);
        auto itHeard = lastHeardTime_.find(planet);
        if (itHeard == lastHeardTime_.end() || currentTime - itHeard->second > 10000) { continue; }
        auto itResp = responses_.find(planet);
        if (itResp == responses_.end()) { continue; }
        const OrreryMessage& resp = itResp->second;
        if (!resp.position.has_value()) { continue; }
        bool atZero = false;
        uint32_t pos = *resp.position;
        if (pos < 6000 || pos > 354000) { atZero = true; }
        if (!atZero || !resp.speed.has_value() || *resp.speed > 10) {
          allArrived = false;
          break;
        }
      }
      if (allArrived) {
        jll_info("%u Planets have aligned, starting one revolution", currentTime);
        waitingForAlignment_ = false;
        SetPosition(Planet::All, std::nullopt);
        SetSpeed(Planet::All, 1000);
        hasPassedHalfway_.clear();
      }
    } else {
      bool allRevolutionCompleted = true;
      for (int i = 0; i < kNumPlanetsWithoutSun; i++) {
        Planet planet = static_cast<Planet>(static_cast<int>(Planet::Mercury) + i);
        auto itHeard = lastHeardTime_.find(planet);
        if (itHeard == lastHeardTime_.end() || currentTime - itHeard->second > 10000) { continue; }
        auto itResp = responses_.find(planet);
        if (itResp == responses_.end()) { continue; }
        const OrreryMessage& resp = itResp->second;
        if (!resp.position.has_value()) { continue; }
        const uint32_t pos = *resp.position;
        if (!hasPassedHalfway_[planet]) {
          if (pos > 180000 && pos < 350000) { hasPassedHalfway_[planet] = true; }
          allRevolutionCompleted = false;
        } else {
          if (pos > 10000) { allRevolutionCompleted = false; }
        }
      }
      if (allRevolutionCompleted) {
        jll_info("%u All planets have completed one revolution, starting Realistic scene", currentTime);
        SetScene(OrreryScene::Realistic);
      }
    }
  }
}

void OrreryLeader::StateChanged(uint8_t pin, bool isClosed) {
  jll_info("%u switch on pin %u is now %s", timeMillis(), pin, (isClosed ? "closed" : "open"));
  if (pin == kSwitch1Pin) {
    HandleSwitch1(isClosed);
  } else if (pin == kSwitch3Pin) {
    HandleSwitch3(isClosed);
  } else if (pin == kSwitch4Pin) {
    HandleSwitch4(isClosed);
  }
}

void OrreryLeader::HandleSwitch1(bool isClosed) { SetScene(isClosed ? OrreryScene::Paused : OrreryScene::Realistic); }

void OrreryLeader::HandleSwitch3(bool isClosed) {
  if (!isClosed && scene_ == OrreryScene::Realistic) {
    lastRandomSceneTime_ = timeMillis();
    nextRandomSceneDuration_ = (UnpredictableRandom::GetNumberBetween(2 * 60, 10 * 60)) * 1000;
    jll_info("%u OrreryLeader scheduling random scene in %u seconds", lastRandomSceneTime_,
             nextRandomSceneDuration_ / 1000);
  } else {
    nextRandomSceneDuration_ = 0;
  }
}

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
