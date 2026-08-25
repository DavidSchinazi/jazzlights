#ifndef JL_PROTOCOL_WIRE_H
#define JL_PROTOCOL_WIRE_H

#include <cstddef>
#include <cstdint>
#include <optional>

#include "jazzlights/protocol/wire_types.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

inline constexpr size_t kMinProtocolPayloadLength = 6 + 2 + 1 + 2 + 4 + 4 + 2;
inline constexpr size_t kMaxProtocolPayloadLength = kMinProtocolPayloadLength + 1 + 3 + 1;

// Serializes `msg` into `payload`. Times are written as durations elapsed since they occurred, relative to `sendTime`
// (which defaults to now). Returns the number of bytes written, or 0 on failure.
size_t WriteProtocolMessage(const ProtocolMessage& msg, uint8_t* payload, size_t maxPayloadLength,
                            OptionalMicroseconds sendTime = std::nullopt);

// Parses `payload`, using `receiptTime` (which defaults to now) to convert the durations on the wire back into times.
// Only the fields carried on the wire are set; the caller is responsible for filling in the transport-specific fields
// (sender, receiptRssi, receiptTime) on the returned message. Returns nullopt and logs on failure.
std::optional<ProtocolMessage> ParseProtocolMessage(const uint8_t* payload, size_t payloadLength,
                                                    OptionalMicroseconds receiptTime = std::nullopt);

}  // namespace jazzlights

#endif  // JL_PROTOCOL_WIRE_H
