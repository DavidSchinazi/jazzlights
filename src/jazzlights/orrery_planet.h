#ifndef JL_ORRERY_PLANET_H
#define JL_ORRERY_PLANET_H

#include "jazzlights/config.h"

#if JL_IS_CONFIG(ORRERY_PLANET)

#include "jazzlights/network/max485_bus.h"
#include "jazzlights/orrery_common.h"
#include "jazzlights/ui/gpio_button.h"
#include "jazzlights/ui/hall_sensor.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

class Player;

class OrreryPlanet
#if !JL_ORRERY_SUN
    : public GpioSwitchInterface,
      public HallSensor::HallSensorInterface
#endif  // !JL_ORRERY_SUN
{
 public:
  static OrreryPlanet* Get();
  void Setup(Player& player);
  void RunLoop(Milliseconds currentTime);
#if !JL_ORRERY_SUN
  // From GpioSwitchInterface.
  void StateChanged(uint8_t pin, bool isClosed) override;

  // From HallSensor::HallSensorInterface.
  void HandleHallSensorChange(uint8_t pin, bool isClosed, Milliseconds timeOfChange) override;
#endif  // !JL_ORRERY_SUN
 private:
  OrreryPlanet();

#if !JL_ORRERY_SUN
  BusId ComputeBusId() const;
  void IncrementStepCount();
#endif  // !JL_ORRERY_SUN

  Player* player_ = nullptr;
  OrreryMessage currentState_ = {};
#if !JL_ORRERY_SUN
  HallSensor hallSensor_;
  std::optional<Milliseconds> timeHallSensorLastOpened_;
  std::optional<Milliseconds> timeHallSensorLastClosed_;
  std::optional<Milliseconds> lastOpenDuration_;
  std::optional<Milliseconds> lastClosedDuration_;
  GpioSwitchHigh switch0_;
  GpioSwitchHigh switch1_;
  GpioSwitchHigh switch2_;
  GpioSwitchHigh switch3_;
  int32_t requestedSpeed_ = 0;
  float actualSpeed_ = 0.0f;
  float roundedSpeed_ = 0.0f;
  Milliseconds lastSpeedUpdateTime_;
  Milliseconds lastStepCountIncrement_;
  float currentSteps_ = 0.0f;
  float positionalSteps_ = 0.0f;
  float stepsPerRev_;
  std::optional<uint32_t> targetPosition_ = std::nullopt;
  bool arrivedAtTarget_ = false;
  bool ignoreNextCalibration_ = true;
#endif  // !JL_ORRERY_SUN
  BusId busId_;
  Max485BusFollower max485BusFollower_;
};

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_PLANET)

#endif  // JL_ORRERY_PLANET_H
