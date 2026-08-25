#ifndef JL_PROTOCOL_WIRE_H
#define JL_PROTOCOL_WIRE_H

#include <cstddef>
#include <cstdint>
#include <optional>

#include "jazzlights/protocol/wire_types.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

// We use two variants of the protocol wire format: one over BLE, and one over UDP. They carry the same fields in the
// same order, but the BLE variant omits the version byte and the sender (which BLE provides out of band), and is the
// only one that carries the extension byte. Which variant is used is selected by the `isBle` parameters below.
inline constexpr size_t kMinBleProtocolPayloadLength = 6 + 2 + 1 + 2 + 4 + 4 + 2;
inline constexpr size_t kMaxBleProtocolPayloadLength = kMinBleProtocolPayloadLength + 1 + 3 + 1;
// The UDP variant adds a version byte and the sender, and carries no extension byte, so it is fixed length.
inline constexpr size_t kUdpProtocolPayloadLength = 1 + 6 + kMinBleProtocolPayloadLength;

// Serializes `msg` into `payload`. Times are written as durations elapsed since they occurred, relative to `sendTime`
// (which defaults to now). Returns the number of bytes written, or 0 on failure.
size_t WriteProtocolMessage(const ProtocolMessage& msg, bool isBle, uint8_t* payload, size_t maxPayloadLength,
                            OptionalMicroseconds sendTime = std::nullopt);

// Parses `payload`, using `receiptTime` (which defaults to now) to convert the durations on the wire back into times.
// Only the fields carried on the wire are set; the caller is responsible for filling in the transport-specific fields
// (receiptRssi, receiptTime, receiptDetails, and - when `isBle` is true - sender) on the returned message. Returns
// nullopt and logs on failure.
std::optional<ProtocolMessage> ParseProtocolMessage(const uint8_t* payload, size_t payloadLength, bool isBle,
                                                    OptionalMicroseconds receiptTime = std::nullopt);

}  // namespace jazzlights

#endif  // JL_PROTOCOL_WIRE_H
