#include "jazzlights/orrery_planet.h"

#if JL_IS_CONFIG(ORRERY_PLANET)

#include <cinttypes>
#include <cstdio>
#include <cstring>

#include "jazzlights/motor.h"
#include "jazzlights/network/max485_bus.h"
#include "jazzlights/network/network.h"
#include "jazzlights/orrery_common.h"
#include "jazzlights/util/log.h"

namespace jazzlights {
namespace {
#if JL_IS_CONTROLLER(M5STAMP_S3)
static constexpr uint8_t kPlanetSwitchPin0 = 12;
static constexpr uint8_t kPlanetSwitchPin1 = 11;
static constexpr uint8_t kPlanetSwitchPin2 = 9;
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

BusId ComputeInitialBusId() {
  // Since switches are not yet initialized when ComputeInitialBusId is called from the initializer list,
  // we could either read the pins directly or assume they are open.
  // However, the previous logic was calling UpdateBusId() which also relied on IsClosed() returning false.
  // For consistency with existing behavior, we'll return the value corresponding to all open.
  return static_cast<BusId>(Planet::Mercury);
}
}  // namespace

OrreryPlanet* OrreryPlanet::Get() {
  static OrreryPlanet sOrreryPlanet;
  return &sOrreryPlanet;
}

OrreryPlanet::OrreryPlanet()
    : switch0_(kPlanetSwitchPin0, *this),
      switch1_(kPlanetSwitchPin1, *this),
      switch2_(kPlanetSwitchPin2, *this),
      busId_(ComputeInitialBusId()),
      max485BusFollower_(UART_NUM_2, kMax485TxPin, kMax485RxPin, busId_) {
  UpdateBusId();
  jll_info("%u OrreryPlanet created with busId %u", timeMillis(), busId_);
}

void OrreryPlanet::UpdateBusId() {
  uint8_t switchesValue = (switch2_.IsClosed() ? 4 : 0) | (switch1_.IsClosed() ? 2 : 0) | (switch0_.IsClosed() ? 1 : 0);
  busId_ = static_cast<BusId>(Planet::Mercury) + switchesValue;
}

void OrreryPlanet::StateChanged(uint8_t pin, bool isClosed) {
  UpdateBusId();
  jll_info("%u OrreryPlanet switch on pin %u is now %s, new busId %u", timeMillis(), pin,
           (isClosed ? "closed" : "open"), busId_);
}

BusId OrreryPlanet::GetBusId() const { return busId_; }

void OrreryPlanet::RunLoop(Milliseconds currentTime) {
  switch0_.RunLoop();
  switch1_.RunLoop();
  switch2_.RunLoop();

  static OwnedBufferU8 readBuffer(1000);
  uint8_t destBusId, srcBusId;
  BufferViewU8 message = max485BusFollower_.ReadMessage(readBuffer, &destBusId, &srcBusId);
  const uint8_t ourPlanetIndex = GetBusId() - static_cast<uint8_t>(Planet::Mercury);
  if (!message.empty()) {
    jll_buffer_info(message, "%u Planet %u read message from %u to %u", currentTime, ourPlanetIndex, srcBusId,
                    destBusId);
  }
  if (message.empty()) { return; }

  NetworkReader reader(message.data(), message.size());
  OrreryMessageType type;
  OrreryMessage msg;
  if (!ReadOrreryMessage(reader, &type, &msg)) { return; }

  if (type == OrreryMessageType::SetSpeed) {
    if (msg.speed.has_value()) {
      jll_info("%u Planet %u applying speed %" PRId32, currentTime, ourPlanetIndex, *msg.speed);
#if JL_MOTOR
      GetMainStepperMotor()->SetSpeed(*msg.speed);
#endif  // JL_MOTOR
      OrreryMessage ackMsg;
      ackMsg.leaderBootId = msg.leaderBootId;
      ackMsg.leaderSequenceNumber = msg.leaderSequenceNumber;
      ackMsg.speed = *msg.speed;
      uint8_t ackBuffer[64];
      NetworkWriter writer(ackBuffer, sizeof(ackBuffer));
      if (WriteOrreryMessage(OrreryMessageType::AckSpeed, ackMsg, writer)) {
        max485BusFollower_.SetMessageToSend(BufferViewU8(ackBuffer, writer.LengthWritten()));
      }
    }
  }
}

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_PLANET)
