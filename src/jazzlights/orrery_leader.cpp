#include "jazzlights/orrery_leader.h"

#if JL_MAX485_BUS

#include <cinttypes>
#include <cstdio>
#include <cstring>

#include "jazzlights/motor.h"
#include "jazzlights/network/max485_bus.h"
#include "jazzlights/util/log.h"

namespace jazzlights {

OrreryLeader* OrreryLeader::Get() {
  static OrreryLeader sOrreryLeader;
  return &sOrreryLeader;
}

void OrreryLeader::SetSpeed(Planet planet, int32_t speed) {
  uint8_t planetIndex = static_cast<uint8_t>(planet);
  if (planetIndex < kNumPlanets) {
    if (speeds_[planetIndex] != speed) {
      speeds_[planetIndex] = speed;
      jll_info("OrreryLeader setting planet %u speed to %" PRId32, planetIndex, speed);
#if JL_BUS_LEADER
      OrreryMessage msg;
      msg.type = OrreryMessageType::SetSpeed;
      msg.planetIndex = planetIndex;
      msg.speed = speed;
      SetMax485BusMessageToSend(BufferViewU8(reinterpret_cast<uint8_t*>(&msg), sizeof(msg)));
#endif  // JL_BUS_LEADER
    }
  }
}

int32_t OrreryLeader::GetSpeed(Planet planet) const {
  int planetIndex = static_cast<int>(planet);
  if (planetIndex >= 0 && planetIndex < kNumPlanets) { return speeds_[planetIndex]; }
  return 0;
}

void OrreryLeader::RunLoop(Milliseconds /*currentTime*/) {
#if JL_BUS_LEADER
  static OwnedBufferU8 readBuffer(1000);
  uint8_t destBusId, srcBusId;
  BufferViewU8 message = ReadMax485BusMessage(readBuffer, &destBusId, &srcBusId);
  if (message.empty() || message.size() < sizeof(OrreryMessage)) { return; }

  const OrreryMessage* msg = reinterpret_cast<const OrreryMessage*>(message.data());

  if (msg->type == OrreryMessageType::AckSpeed) {
    jll_info("OrreryLeader received ack for planet %u: %" PRId32, msg->planetIndex, msg->speed);
  }
#endif  // JL_BUS_LEADER
}

}  // namespace jazzlights

#endif  // JL_MAX485_BUS
