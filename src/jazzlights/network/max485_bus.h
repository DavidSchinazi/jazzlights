#ifndef JL_NETWORK_MAX485_BUS_H
#define JL_NETWORK_MAX485_BUS_H

#include "jazzlights/config.h"
#include "jazzlights/util/buffer.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

#if JL_MAX485_BUS

using BusId = uint8_t;

void SetupMax485Bus();

void RunMax485Bus(Milliseconds currentTime);

void SetMax485BusMessageToSend(const BufferViewU8 message);

BufferViewU8 ReadMax485BusMessage(OwnedBufferU8& readMessageBuffer, uint8_t* destBusId, uint8_t* srcBusId);

#else   // JL_MAX485_BUS
static inline void SetupMax485Bus() {}
static inline void RunMax485Bus(Milliseconds currentTime) { (void)currentTime; }
static inline void SetMax485BusMessageToSend(const BufferViewU8 message) { (void)message; }
static inline BufferViewU8 ReadMax485BusMessage(OwnedBufferU8& readMessageBuffer, uint8_t* destBusId,
                                                uint8_t* srcBusId) {
  (void)readMessageBuffer;
  (void)destBusId;
  (void)srcBusId;
  return BufferViewU8();
}
#endif  // JL_MAX485_BUS

}  // namespace jazzlights

#endif  // JL_NETWORK_MAX485_BUS_H
