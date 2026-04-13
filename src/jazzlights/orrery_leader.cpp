#include "jazzlights/orrery_leader.h"

#if JL_IS_CONFIG(ORRERY_LEADER)

#include <cinttypes>
#include <cstdio>
#include <cstring>

#include "jazzlights/motor.h"
#include "jazzlights/network/max485_bus.h"
#include "jazzlights/network/network.h"
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

OrreryLeader::OrreryLeader() : max485BusLeader_(UART_NUM_2, kMax485TxPin, kMax485RxPin) {}

void OrreryLeader::SetSpeed(Planet planet, int32_t speed) {
  uint8_t planetIndex = static_cast<uint8_t>(planet) - static_cast<uint8_t>(Planet::Mercury);
  if (planetIndex < kNumPlanets) {
    if (speeds_[planetIndex] != speed) {
      speeds_[planetIndex] = speed;
      jll_info("OrreryLeader setting planet %u speed to %" PRId32, planetIndex, speed);
#if JL_BUS_LEADER
      uint8_t messageBuffer[6];
      NetworkWriter writer(messageBuffer, sizeof(messageBuffer));
      if (writer.WriteUint8(static_cast<uint8_t>(OrreryMessageType::SetSpeed)) && writer.WriteUint8(planetIndex) &&
          writer.WriteInt32(speed)) {
        max485BusLeader_.SetMessageToSend(static_cast<BusId>(planet),
                                          BufferViewU8(messageBuffer, writer.LengthWritten()));
      }
#endif  // JL_BUS_LEADER
    }
  }
}

int32_t OrreryLeader::GetSpeed(Planet planet) const {
  uint8_t planetIndex = static_cast<uint8_t>(planet) - static_cast<uint8_t>(Planet::Mercury);
  if (planetIndex >= 0 && planetIndex < kNumPlanets) { return speeds_[planetIndex]; }
  return 0;
}

void OrreryLeader::RunLoop(Milliseconds /*currentTime*/) {
#if JL_BUS_LEADER
  static OwnedBufferU8 readBuffer(1000);
  uint8_t destBusId, srcBusId;
  BufferViewU8 message = max485BusLeader_.ReadMessage(readBuffer, &destBusId, &srcBusId);
  if (message.empty()) { return; }

  NetworkReader reader(message.data(), message.size());
  uint8_t type;
  if (!reader.ReadUint8(&type)) { return; }

  if (type == static_cast<uint8_t>(OrreryMessageType::AckSpeed)) {
    uint8_t planetIndex;
    int32_t speed;
    if (reader.ReadUint8(&planetIndex) && reader.ReadInt32(&speed)) {
      jll_info("OrreryLeader received ack for planet %u: %" PRId32, planetIndex, speed);
    }
  }
#endif  // JL_BUS_LEADER
}

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_LEADER)
