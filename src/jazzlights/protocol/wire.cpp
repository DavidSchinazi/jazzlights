#include "jazzlights/protocol/wire.h"

#include "jazzlights/protocol/reader.h"
#include "jazzlights/protocol/writer.h"
#include "jazzlights/util/log.h"

namespace jazzlights {
namespace {

// Wire format of a ProtocolMessage. Fields marked "UDP only" are absent when `isBle` is true, and fields marked
// "BLE only" are absent when it is false.
// version: 1 (UDP only, high nibble must be kUdpVersion)
// originator: 6
// sender: 6 (UDP only, BLE learns the sender from the advertisement instead)
// precedence: 2
// numHops: 1
// originationTime: 2
// currentPattern: 4
// nextPattern: 4
// patternTime: 2
// [extensionByte: 1 (BLE only, 0x80 = isCreature, 0x40 = isPartying, 0x20 = orreryScene)]
// [creatureRGB: 3 (BLE only)]
// [orreryScene: 1 (BLE only)]

constexpr uint8_t kUdpVersion = 0x10;
constexpr uint8_t kUdpVersionMask = 0xF0;

enum ExtensionByteFlag : uint8_t {
  kExtensionByteFlagIsCreature = 0x80,
  kExtensionByteFlagIsPartying = 0x40,
  kExtensionByteFlagHasOrreryScene = 0x20,
};

}  // namespace

size_t WriteProtocolMessage(const ProtocolMessage& msg, bool isBle, uint8_t* payload, size_t maxPayloadLength,
                            OptionalMicroseconds sendTime) {
  const Microseconds sendTimeUs = sendTime.value_or(TimeMicros());
  ProtocolWriter writer(payload, maxPayloadLength);
  if (!isBle) {
    if (!writer.WriteUint8(kUdpVersion)) {
      jll_error("Failed to write version");
      return 0;
    }
  }
  if (!writer.WriteNetworkDeviceId(msg.originator)) {
    jll_error("Failed to write originator");
    return 0;
  }
  if (!isBle) {
    if (!writer.WriteNetworkDeviceId(msg.sender)) {
      jll_error("Failed to write sender");
      return 0;
    }
  }
  if (!writer.WriteUint16(msg.precedence)) {
    jll_error("Failed to write precedence");
    return 0;
  }
  if (!writer.WriteUint8(msg.numHops)) {
    jll_error("Failed to write numHops");
    return 0;
  }
  if (!writer.WriteTimeSinceMs16(msg.lastOriginationTime, sendTimeUs)) {
    jll_error("Failed to write originationTimeDelta");
    return 0;
  }
  if (!writer.WriteUint32(msg.currentPattern)) {
    jll_error("Failed to write currentPattern");
    return 0;
  }
  if (!writer.WriteUint32(msg.nextPattern)) {
    jll_error("Failed to write nextPattern");
    return 0;
  }
  if (!writer.WriteTimeSinceMs16(msg.currentPatternStartTime, sendTimeUs)) {
    jll_error("Failed to write patternTimeDelta");
    return 0;
  }
  // The extension byte is only sent over BLE.
  if (!isBle) { return writer.LengthWritten(); }
#if JL_IS_CONFIG(CREATURE)
  if (msg.isCreature) {
    uint8_t extensionByte = kExtensionByteFlagIsCreature;
    if (msg.isPartying) { extensionByte |= kExtensionByteFlagIsPartying; }
    if (!writer.WriteUint8(extensionByte)) {
      jll_error("Failed to write creature extensionByte");
      return 0;
    }
    uint8_t creatureRed = (msg.creatureColor >> 16) & 0xFF;
    uint8_t creatureGreen = (msg.creatureColor >> 8) & 0xFF;
    uint8_t creatureBlue = msg.creatureColor & 0xFF;
    if (!writer.WriteUint8(creatureRed) || !writer.WriteUint8(creatureGreen) || !writer.WriteUint8(creatureBlue)) {
      jll_error("Failed to write creature RGB");
      return 0;
    }
  }
#else   // CREATURE
  if (msg.orrerySceneId) {
    uint8_t extensionByte = kExtensionByteFlagHasOrreryScene;
    if (!writer.WriteUint8(extensionByte)) {
      jll_error("Failed to write extensionByte");
      return 0;
    }
    if (!writer.WriteUint8(*msg.orrerySceneId)) {
      jll_error("Failed to write orrerySceneId");
      return 0;
    }
  }
#endif  // CREATURE
  return writer.LengthWritten();
}

std::optional<ProtocolMessage> ParseProtocolMessage(const uint8_t* payload, size_t payloadLength, bool isBle,
                                                    OptionalMicroseconds receiptTime) {
  const size_t minPayloadLength = isBle ? kMinBleProtocolPayloadLength : kUdpProtocolPayloadLength;
  if (payloadLength < minPayloadLength) {
    jll_debug("Ignoring received message with unexpected length %zu < %zu", payloadLength, minPayloadLength);
    return std::nullopt;
  }
  const Microseconds receiptTimeUs = receiptTime.value_or(TimeMicros());
  ProtocolReader reader(payload, payloadLength);
  ProtocolMessage msg;
  if (!isBle) {
    uint8_t version;
    if (!reader.ReadUint8(&version)) {
      jll_error("Failed to parse version");
      return std::nullopt;
    }
    if ((version & kUdpVersionMask) != kUdpVersion) {
      jll_debug("Ignoring received message with unexpected prefix %02x", version);
      return std::nullopt;
    }
  }
  if (!reader.ReadNetworkDeviceId(&msg.originator)) {
    jll_error("Failed to parse originator");
    return std::nullopt;
  }
  if (!isBle) {
    if (!reader.ReadNetworkDeviceId(&msg.sender)) {
      jll_error("Failed to parse sender");
      return std::nullopt;
    }
  }
  if (!reader.ReadUint16(&msg.precedence)) {
    jll_error("Failed to parse precedence");
    return std::nullopt;
  }
  if (!reader.ReadUint8(&msg.numHops)) {
    jll_error("Failed to parse numHops");
    return std::nullopt;
  }
  if (!reader.ReadTimeSinceMs16(&msg.lastOriginationTime, receiptTimeUs)) {
    jll_error("Failed to parse originationTimeDelta");
    return std::nullopt;
  }
  if (!reader.ReadPatternBits(&msg.currentPattern)) {
    jll_error("Failed to parse currentPattern");
    return std::nullopt;
  }
  if (!reader.ReadPatternBits(&msg.nextPattern)) {
    jll_error("Failed to parse nextPattern");
    return std::nullopt;
  }
  if (!reader.ReadTimeSinceMs16(&msg.currentPatternStartTime, receiptTimeUs)) {
    jll_error("Failed to parse patternTimeDelta");
    return std::nullopt;
  }
  // The extension byte is only sent over BLE. Over UDP we ignore any trailing bytes.
  if (!isBle) { return msg; }
  uint8_t extensionByte = 0x00;
  if (!reader.Done()) {
    if (!reader.ReadUint8(&extensionByte)) {
      jll_error("Failed to parse extensionByte");
      return std::nullopt;
    }
  }
  if ((extensionByte & kExtensionByteFlagIsCreature) != 0) {
    uint8_t creatureRed, creatureGreen, creatureBlue;
    if (!reader.ReadUint8(&creatureRed) || !reader.ReadUint8(&creatureGreen) || !reader.ReadUint8(&creatureBlue)) {
      jll_error("Failed to parse creature RGB");
      return std::nullopt;
    }
#if JL_IS_CONFIG(CREATURE)
    msg.isCreature = true;
    msg.isPartying = (extensionByte & kExtensionByteFlagIsPartying) != 0;
    msg.creatureColor = (creatureRed << 16) | (creatureGreen << 8) | creatureBlue;
#endif  // CREATURE
  }
  if ((extensionByte & kExtensionByteFlagHasOrreryScene) != 0) {
    uint8_t orrerySceneId;
    if (!reader.ReadUint8(&orrerySceneId)) {
      jll_error("Failed to parse orrerySceneId");
      return std::nullopt;
    }
    msg.orrerySceneId = orrerySceneId;
  }
  return msg;
}

}  // namespace jazzlights
