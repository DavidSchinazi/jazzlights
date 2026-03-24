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

OrreryPlanet* OrreryPlanet::Get() {
  static OrreryPlanet sOrreryPlanet;
  return &sOrreryPlanet;
}

void OrreryPlanet::RunLoop(Milliseconds /*currentTime*/) {
  static OwnedBufferU8 readBuffer(1000);
  uint8_t destBusId, srcBusId;
  BufferViewU8 message = ReadMax485BusMessage(readBuffer, &destBusId, &srcBusId);
  if (!message.empty()) {
    jll_buffer_info(message, STRINGIFY(JL_ROLE) " planet read message from %d to %d", static_cast<int>(srcBusId),
                    static_cast<int>(destBusId));
  }
  if (message.empty() || message.size() < sizeof(OrreryMessage)) { return; }

  const OrreryMessage* msg = reinterpret_cast<const OrreryMessage*>(message.data());

  if (msg->type == OrreryMessageType::SetSpeed) {
    if (msg->planetIndex < OrreryLeader::kNumPlanets) {
      jll_info("OrreryFollower applying speed %" PRId32 " for planet %u", msg->speed, msg->planetIndex);
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
