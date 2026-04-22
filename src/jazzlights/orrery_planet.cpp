#include "jazzlights/orrery_planet.h"

#if JL_IS_CONFIG(ORRERY_PLANET)

#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "jazzlights/effect/planet.h"
#include "jazzlights/motor.h"
#include "jazzlights/network/max485_bus.h"
#include "jazzlights/network/network.h"
#include "jazzlights/orrery_common.h"
#include "jazzlights/player.h"
#include "jazzlights/ui/hall_sensor.h"
#include "jazzlights/util/log.h"

namespace jazzlights {
namespace {
#if JL_IS_CONTROLLER(M5STAMP_S3)
static constexpr uint8_t kPlanetSwitchPin0 = 12;
static constexpr uint8_t kPlanetSwitchPin1 = 11;
static constexpr uint8_t kPlanetSwitchPin2 = 9;
static constexpr uint8_t kPlanetSwitchPin3 = 8;
static constexpr uint8_t kHallSensorPin = 4;
#else
#error "Unexpected controller"
#endif

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

// This value was measured empirically on the orrery.
constexpr float kStartupStepsPerRev = 17000.0f;

}  // namespace

OrreryPlanet* OrreryPlanet::Get() {
  static OrreryPlanet sOrreryPlanet;
  return &sOrreryPlanet;
}

void OrreryPlanet::Setup(Player& player) {
  player_ = &player;
  player_->set_brightness(kDefaultPlanetBrightness);
  currentState_.ledBrightness = kDefaultPlanetBrightness;
  player_->SetPlanetPattern(kPlanetPattern);
  currentState_.ledPattern = kPlanetPattern;
  player_->setBasePrecedence(kDefaultPlanetBasePrecedence);
  currentState_.ledBasePrecedence = kDefaultPlanetBasePrecedence;
  player_->setPrecedenceGain(kDefaultPlanetPrecedenceGain);
  currentState_.ledPrecedenceGain = kDefaultPlanetPrecedenceGain;
}

OrreryPlanet::OrreryPlanet()
    : hallSensor_(kHallSensorPin, *this),
      switch0_(kPlanetSwitchPin0, *this),
      switch1_(kPlanetSwitchPin1, *this),
      switch2_(kPlanetSwitchPin2, *this),
      switch3_(kPlanetSwitchPin3, *this),
      busId_(ComputeBusId()),
      max485BusFollower_(UART_NUM_2, kMax485TxPin, kMax485RxPin, busId_),
      lastSpeedUpdateTime_(timeMillis()),
      lastStepCountIncrement_(timeMillis()),
      stepsPerRev_(kStartupStepsPerRev) {
  jll_info("%u OrreryPlanet created with busId %u", timeMillis(), busId_);
  PlanetEffect::Get()->SetPlanet(static_cast<Planet>(busId_));
  currentState_.type = OrreryMessageType::FollowerResponse;
}

BusId OrreryPlanet::ComputeBusId() const {
  uint8_t switchesValue = (switch2_.IsClosed() ? 4 : 0) | (switch1_.IsClosed() ? 2 : 0) | (switch0_.IsClosed() ? 1 : 0);
  return static_cast<BusId>(Planet::Mercury) + switchesValue;
}

void OrreryPlanet::StateChanged(uint8_t pin, bool isClosed) {
  if (pin == kPlanetSwitchPin3) {
    jll_info("%u OrreryPlanet motor direction switch on pin %u is now %s", timeMillis(), pin,
             (isClosed ? "closed" : "open"));
    GetMainStepperMotor()->SetRunBackwards(isClosed);
    return;
  }
  const BusId busId = ComputeBusId();
  if (busId == busId_) { return; }
  busId_ = busId;
  max485BusFollower_.SetBusIdSelf(busId_);

  PlanetEffect::Get()->SetPlanet(static_cast<Planet>(busId_));
  jll_info("%u OrreryPlanet planet switch on pin %u is now %s, new planet %s (busId %u)", timeMillis(), pin,
           (isClosed ? "closed" : "open"), GetPlanetName(static_cast<Planet>(busId_)), busId_);
}

void OrreryPlanet::IncrementStepCount() {
  Milliseconds currentTime = timeMillis();
  Milliseconds timeSinceLastStepCountIncrement = currentTime - lastStepCountIncrement_;
  if (timeSinceLastStepCountIncrement <= 0) { return; }
  currentSteps_ += (roundedSpeed_ * timeSinceLastStepCountIncrement) / 1000.0f;
  lastStepCountIncrement_ = currentTime;
}

void OrreryPlanet::HandleHallSensorChange(uint8_t pin, bool isClosed, Milliseconds timeOfChange) {
  jll_info("%u Hall sensor at pin %d is now %s", timeOfChange, static_cast<int>(pin), (isClosed ? "closed" : "open"));
  PlanetEffect::Get()->SetHallSensorClosed(isClosed);
  if (isClosed) {
    IncrementStepCount();
    if (ignoreNextCalibration_) {
      ignoreNextCalibration_ = false;
      jll_info("%u Ignoring calibration stepsPerRev %.2f, staying with %.2f", timeOfChange, std::abs(currentSteps_),
               stepsPerRev_);
    } else if (currentSteps_ != 0) {
      float previousStepsPerRev = stepsPerRev_;
      if (currentState_.calibration.has_value()) {
        static constexpr float kCalibrationSmoothing = 0.5f;
        stepsPerRev_ = kCalibrationSmoothing * stepsPerRev_ + (1.0f - kCalibrationSmoothing) * std::abs(currentSteps_);
        jll_info("%u Calibrated stepsPerRev: new measurement %.2f smoothed from %.2f to %.2f", timeOfChange,
                 std::abs(currentSteps_), previousStepsPerRev, stepsPerRev_);
      } else {
        stepsPerRev_ = std::abs(currentSteps_);
        jll_info("%u First calibration of stepsPerRev from %.2f to %.2f", timeOfChange, previousStepsPerRev,
                 stepsPerRev_);
      }
      currentState_.calibration = static_cast<uint32_t>(std::round(stepsPerRev_));
      currentSteps_ = 0;
    }
    if (timeHallSensorLastOpened_.has_value()) { lastOpenDuration_ = timeOfChange - *timeHallSensorLastOpened_; }
    timeHallSensorLastClosed_ = timeOfChange;
  } else {
    if (timeHallSensorLastClosed_.has_value()) { lastClosedDuration_ = timeOfChange - *timeHallSensorLastClosed_; }
    timeHallSensorLastOpened_ = timeOfChange;
  }
}

void OrreryPlanet::RunLoop(Milliseconds currentTime) {
  hallSensor_.RunLoop();
  switch0_.RunLoop();
  switch1_.RunLoop();
  switch2_.RunLoop();
  switch3_.RunLoop();

  const Milliseconds dt = currentTime - lastSpeedUpdateTime_;
  if (dt > 0) {
    positionalSteps_ = currentSteps_ + ((roundedSpeed_ * (currentTime - lastStepCountIncrement_)) / 1000.0f);
    while (positionalSteps_ >= stepsPerRev_) { positionalSteps_ -= stepsPerRev_; }
    while (positionalSteps_ < 0) { positionalSteps_ += stepsPerRev_; }

    int32_t effectiveRequestedSpeed = requestedSpeed_;
    if (arrivedAtTarget_) {
      effectiveRequestedSpeed = 0;
      actualSpeed_ = 0;
    } else if (targetPosition_.has_value()) {
      const float targetSteps = (*targetPosition_ / 360000.0f) * stepsPerRev_;
      float stepsToGo = targetSteps - positionalSteps_;
      while (stepsToGo < (-stepsPerRev_ / 100.0f)) { stepsToGo += stepsPerRev_; }

      static Milliseconds lastLogTime = -1;
      if (lastLogTime < 0 || currentTime - lastLogTime > 1000) {
        lastLogTime = currentTime;
        jll_info("%u targetSteps %f currentSteps %f positionalSteps %f stepsToGo %f actualSpeed_ %f", currentTime,
                 targetSteps, currentSteps_, positionalSteps_, stepsToGo, actualSpeed_);
      }

      if (stepsToGo < (stepsPerRev_ / 100.0f) && std::abs(actualSpeed_) < 10.0f) {
        effectiveRequestedSpeed = 0;
        actualSpeed_ = 0;
        arrivedAtTarget_ = true;
        jll_info(
            "%u Planet %s arrived at target position %lu targetSteps %f currentSteps %f positionalSteps %f "
            "stepsToGo %f actualSpeed %f",
            currentTime, GetPlanetName(static_cast<Planet>(busId_)), *targetPosition_, targetSteps, currentSteps_,
            positionalSteps_, stepsToGo, actualSpeed_);

      } else {
        // Distance to stop at current deceleration (250 steps/s^2) is v^2 / 500.
        const float stop_distance = (actualSpeed_ * actualSpeed_) / 500.0f;

        static Milliseconds lastLogTime3 = -1;
        if (lastLogTime3 < 0 || currentTime - lastLogTime3 > 1000) {
          lastLogTime3 = currentTime;
          jll_info("%u targetSteps %f currentSteps %f positionalSteps %f stepsToGo %f actualSpeed_ %f stop_distance %f",
                   currentTime, targetSteps, currentSteps_, positionalSteps_, stepsToGo, actualSpeed_, stop_distance);
        }
        if (stepsToGo <= stop_distance) { effectiveRequestedSpeed = 0; }
      }
    }

    const float maxChange = 250.0f * dt / 1000.0f;
    if (actualSpeed_ < static_cast<float>(effectiveRequestedSpeed)) {
      actualSpeed_ = std::min(static_cast<float>(effectiveRequestedSpeed), actualSpeed_ + maxChange);
    } else if (actualSpeed_ > static_cast<float>(effectiveRequestedSpeed)) {
      actualSpeed_ = std::max(static_cast<float>(effectiveRequestedSpeed), actualSpeed_ - maxChange);
    }
    lastSpeedUpdateTime_ = currentTime;

    roundedSpeed_ = std::round(actualSpeed_);
    constexpr int32_t kMaxMotorSpeedHz = 10000;
    if (roundedSpeed_ > kMaxMotorSpeedHz) { roundedSpeed_ = kMaxMotorSpeedHz; }
    if (roundedSpeed_ < -kMaxMotorSpeedHz) { roundedSpeed_ = -kMaxMotorSpeedHz; }

    static Milliseconds lastLogTime2 = -1;
    if (lastLogTime2 < 0 || currentTime - lastLogTime2 > 1000) {
      lastLogTime2 = currentTime;
      jll_info("%u Setting roundedSpeed to %f; currentSteps %f positionalSteps %f actualSpeed %f stepsPerRev %f",
               currentTime, roundedSpeed_, currentSteps_, positionalSteps_, actualSpeed_, stepsPerRev_);
    }
    if (!currentState_.speed.has_value() || roundedSpeed_ != *currentState_.speed) {
      // Ignore calibration if the motor stops or reverses direction.
      int wasForward = 0;
      if (currentState_.speed.has_value()) {
        if (*currentState_.speed > 0) {
          wasForward = 1;
        } else if (*currentState_.speed < 0) {
          wasForward = -1;
        }
      }
      int isForward = 0;
      if (roundedSpeed_ > 0) {
        isForward = 1;
      } else if (roundedSpeed_ < 0) {
        isForward = -1;
      }
      if (wasForward * isForward <= 0) { ignoreNextCalibration_ = true; }
      IncrementStepCount();
#if JL_MOTOR
      GetMainStepperMotor()->SetSpeed(static_cast<int32_t>(roundedSpeed_));
#endif  // JL_MOTOR
      currentState_.speed = roundedSpeed_;
    }
    currentState_.position = static_cast<uint32_t>((positionalSteps_ / stepsPerRev_) * 360000.0f);
  }

  BusId destBusId, srcBusId;
  OrreryMessage msg;
  while (max485BusFollower_.ReadMessage(&msg, &destBusId, &srcBusId)) {
    const char* ourPlanetName = GetPlanetName(static_cast<Planet>(busId_));

    if (msg.type == OrreryMessageType::LeaderCommand) {
      currentState_.leaderBootId = msg.leaderBootId;
      currentState_.leaderSequenceNumber = msg.leaderSequenceNumber;
      if (msg.speed.has_value()) {
        const int32_t targetFrequency = std::round((*msg.speed / 60000.0f) * stepsPerRev_);
        if (targetFrequency != requestedSpeed_) {
          jll_info("%u Planet %s requested milli-RPM %" PRId32 " (target frequency %" PRId32 "Hz)", currentTime,
                   ourPlanetName, *msg.speed, targetFrequency);
          requestedSpeed_ = targetFrequency;
        }
      }
      if (!msg.position.has_value()) {
        targetPosition_ = std::nullopt;
        arrivedAtTarget_ = false;
      } else if (msg.position.has_value() && (!targetPosition_.has_value() || *msg.position != *targetPosition_)) {
        jll_info("%u Planet %s requested position %" PRIu32, currentTime, ourPlanetName, *msg.position);
        targetPosition_ = msg.position;
        arrivedAtTarget_ = false;
      }
      if (msg.ledBrightness.has_value() && msg.ledBrightness != currentState_.ledBrightness) {
        jll_info("%u Planet %s applying brightness %u", currentTime, ourPlanetName, *msg.ledBrightness);
        if (player_ != nullptr) { player_->set_brightness(*msg.ledBrightness); }
        currentState_.ledBrightness = *msg.ledBrightness;
      }
      if (msg.ledPattern.has_value() && msg.ledPattern != currentState_.ledPattern) {
        if (player_ != nullptr && player_->GetPlanetPattern() != *msg.ledPattern) {
          jll_info("%u Planet %s applying pattern %08x from leader", currentTime, ourPlanetName, *msg.ledPattern);
          player_->SetPlanetPattern(*msg.ledPattern);
        }
        currentState_.ledPattern = *msg.ledPattern;
      }
      if (msg.ledBasePrecedence.has_value() && msg.ledBasePrecedence != currentState_.ledBasePrecedence) {
        jll_info("%u Planet %s applying base precedence %u", currentTime, ourPlanetName, *msg.ledBasePrecedence);
        if (player_ != nullptr) { player_->setBasePrecedence(*msg.ledBasePrecedence); }
        currentState_.ledBasePrecedence = *msg.ledBasePrecedence;
      }
      if (msg.ledPrecedenceGain.has_value() && msg.ledPrecedenceGain != currentState_.ledPrecedenceGain) {
        jll_info("%u Planet %s applying precedence gain %u", currentTime, ourPlanetName, *msg.ledPrecedenceGain);
        if (player_ != nullptr) { player_->setPrecedenceGain(*msg.ledPrecedenceGain); }
        currentState_.ledPrecedenceGain = *msg.ledPrecedenceGain;
      }
      if (msg.calibration.has_value() && !currentState_.calibration.has_value()) {
        if (static_cast<float>(*msg.calibration) != stepsPerRev_) {
          jll_info("%u Planet %s applying calibration %" PRIu32 " from leader", currentTime, ourPlanetName,
                   *msg.calibration);
          stepsPerRev_ = *msg.calibration;
        }
      }

      currentState_.timeHallSensorLastOpened = timeHallSensorLastOpened_;
      currentState_.timeHallSensorLastClosed = timeHallSensorLastClosed_;
      currentState_.lastOpenDuration = lastOpenDuration_;
      currentState_.lastClosedDuration = lastClosedDuration_;
      max485BusFollower_.SetMessageToSend(currentState_);
    }
  }
}

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_PLANET)
