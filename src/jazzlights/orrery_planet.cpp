#include "jazzlights/orrery_planet.h"

#if JL_IS_CONFIG(ORRERY_PLANET)

#include <cinttypes>
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
static constexpr uint8_t kHallSensorPin = 15;
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

#if JL_HALL_SENSOR
HallSensor* GetHallSensor() {
  static HallSensor sHallSensor(kHallSensorPin);
  return &sHallSensor;
}
#endif  // JL_HALL_SENSOR

}  // namespace

OrreryPlanet* OrreryPlanet::Get() {
  static OrreryPlanet sOrreryPlanet;
  return &sOrreryPlanet;
}

void OrreryPlanet::Setup(Player& player) {
  player_ = &player;
  player_->set_brightness(kDefaultPlanetBrightness);
#if JL_HALL_SENSOR
  (void)GetHallSensor();
#endif  // JL_HALL_SENSOR
}

OrreryPlanet::OrreryPlanet()
    : switch0_(kPlanetSwitchPin0, *this),
      switch1_(kPlanetSwitchPin1, *this),
      switch2_(kPlanetSwitchPin2, *this),
      busId_(ComputeBusId()),
      max485BusFollower_(UART_NUM_2, kMax485TxPin, kMax485RxPin, busId_) {
  jll_info("%u OrreryPlanet created with busId %u", timeMillis(), busId_);
  PlanetEffect::Get()->SetPlanet(static_cast<Planet>(busId_));
  currentState_.type = OrreryMessageType::FollowerResponse;
}

BusId OrreryPlanet::ComputeBusId() const {
  uint8_t switchesValue = (switch2_.IsClosed() ? 4 : 0) | (switch1_.IsClosed() ? 2 : 0) | (switch0_.IsClosed() ? 1 : 0);
  return static_cast<BusId>(Planet::Mercury) + switchesValue;
}

void OrreryPlanet::StateChanged(uint8_t pin, bool isClosed) {
  const BusId busId = ComputeBusId();
  if (busId == busId_) { return; }
  busId_ = busId;
  max485BusFollower_.SetBusIdSelf(busId_);

  PlanetEffect::Get()->SetPlanet(static_cast<Planet>(busId_));
  jll_info("%u OrreryPlanet switch on pin %u is now %s, new planet %s (busId %u)", timeMillis(), pin,
           (isClosed ? "closed" : "open"), GetPlanetName(static_cast<Planet>(busId_)), busId_);
}

void OrreryPlanet::RunLoop(Milliseconds currentTime) {
  switch0_.RunLoop();
  switch1_.RunLoop();
  switch2_.RunLoop();

  std::optional<Milliseconds> timeHallSensorLastOpened;
#if JL_HALL_SENSOR
  GetHallSensor()->RunLoop();
  timeHallSensorLastOpened = GetHallSensor()->GetTimeLastOpened();
#endif  // JL_HALL_SENSOR

  BusId destBusId, srcBusId;
  OrreryMessage msg;
  if (!max485BusFollower_.ReadMessage(&msg, &destBusId, &srcBusId)) { return; }
  const char* ourPlanetName = GetPlanetName(static_cast<Planet>(busId_));

  if (msg.type == OrreryMessageType::LeaderCommand) {
    currentState_.leaderBootId = msg.leaderBootId;
    currentState_.leaderSequenceNumber = msg.leaderSequenceNumber;
    if (msg.speed.has_value()) {
      jll_info("%u Planet %s applying speed %" PRId32, currentTime, ourPlanetName, *msg.speed);
#if JL_MOTOR
      GetMainStepperMotor()->SetSpeed(*msg.speed);
#endif  // JL_MOTOR
      currentState_.speed = *msg.speed;
    }
    if (msg.ledBrightness.has_value()) {
      jll_info("%u Planet %s applying brightness %u", currentTime, ourPlanetName, *msg.ledBrightness);
      if (player_ != nullptr) { player_->set_brightness(*msg.ledBrightness); }
      currentState_.ledBrightness = *msg.ledBrightness;
    }

    max485BusFollower_.SetMessageToSend(currentState_);
  }
}

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_PLANET)
