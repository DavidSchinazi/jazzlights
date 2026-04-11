#include "jazzlights/orrery_planet.h"

#if JL_MAX485_BUS

#include <cinttypes>
#include <cstdio>
#include <cstring>

#include "jazzlights/motor.h"
#include "jazzlights/network/max485_bus.h"
#include "jazzlights/orrery_leader.h"
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
}  // namespace

OrreryPlanet* OrreryPlanet::Get() {
  static OrreryPlanet sOrreryPlanet;
  return &sOrreryPlanet;
}

OrreryPlanet::OrreryPlanet() : switch0_(kPlanetSwitchPin0), switch1_(kPlanetSwitchPin1), switch2_(kPlanetSwitchPin2) {
  jll_info("OrreryPlanet created with busId %u", GetBusId());
}

BusId OrreryPlanet::GetBusId() const {
  uint8_t switchesValue = (switch2_.IsClosed() ? 4 : 0) | (switch1_.IsClosed() ? 2 : 0) | (switch0_.IsClosed() ? 1 : 0);
  return static_cast<BusId>(Planet::Mercury) + switchesValue;
}

void OrreryPlanet::RunLoop(Milliseconds currentTime) {
  switch0_.RunLoop();
  switch1_.RunLoop();
  switch2_.RunLoop();

  static OwnedBufferU8 readBuffer(1000);
  uint8_t destBusId, srcBusId;
  BufferViewU8 message = ReadMax485BusMessage(readBuffer, &destBusId, &srcBusId);
  const uint8_t ourPlanetIndex = GetBusId() - static_cast<uint8_t>(Planet::Mercury);
  if (!message.empty()) {
    jll_buffer_info(message, "Planet %u read message from %u to %u", ourPlanetIndex, srcBusId, destBusId);
  }
  if (message.empty() || message.size() < sizeof(OrreryMessage)) { return; }

  const OrreryMessage* msg = reinterpret_cast<const OrreryMessage*>(message.data());

  if (msg->type == OrreryMessageType::SetSpeed) {
    if (msg->planetIndex == ourPlanetIndex) {
      jll_info("Planet %u applying speed %" PRId32, ourPlanetIndex, msg->speed);
#if JL_MOTOR
      GetMainStepperMotor()->SetSpeed(msg->speed);
#endif  // JL_MOTOR
      OrreryMessage ack;
      ack.type = OrreryMessageType::AckSpeed;
      ack.planetIndex = msg->planetIndex;
      ack.speed = msg->speed;
      SetMax485BusMessageToSend(BufferViewU8(reinterpret_cast<uint8_t*>(&ack), sizeof(ack)));
    }
  }
}

}  // namespace jazzlights

#endif  // JL_MAX485_BUS
