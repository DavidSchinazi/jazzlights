#include "jazzlights/network/ble_payload.h"

#include "jazzlights/util/log.h"

namespace jazzlights {
namespace {

// originator: 6
// precedence: 2
// numHops: 1
// originationTime: 2
// currentPattern: 4
// nextPattern: 4
// patternTime: 2
// extensionByte: 1 (0x80 = isCreature, 0x40 = isPartying)
// [creatureRGB: 3]
// [orreryScene: 1]

#if JL_IS_CONFIG(CREATURE)
constexpr uint8_t kMinPayloadLength = 6 + 2 + 1 + 2 + 4 + 4 + 2;
constexpr uint8_t kMaxPayloadLength = kMinPayloadLength + 1 + 3;
#else   // CREATURE
constexpr uint8_t kOriginatorOffset = 0;
constexpr uint8_t kPrecedenceOffset = kOriginatorOffset + 6;
constexpr uint8_t kNumHopsOffset = kPrecedenceOffset + 2;
constexpr uint8_t kOriginationTimeOffset = kNumHopsOffset + 1;
constexpr uint8_t kCurrentPatternOffset = kOriginationTimeOffset + 2;
constexpr uint8_t kNextPatternOffset = kCurrentPatternOffset + 4;
constexpr uint8_t kPatternTimeOffset = kNextPatternOffset + 4;
constexpr uint8_t kExtensionByteOffset = kPatternTimeOffset + 2;
constexpr uint8_t kOrrerySceneOffset = kExtensionByteOffset + 1;
constexpr uint8_t kMinPayloadLength = kExtensionByteOffset;
constexpr uint8_t kMaxPayloadLength = kOrrerySceneOffset + 1;
#endif  // CREATURE

constexpr uint8_t kExtensionByteFlagIsCreature = 0x80;
constexpr uint8_t kExtensionByteFlagIsPartying = 0x40;
constexpr uint8_t kExtensionByteFlagHasOrreryScene = 0x20;

// Empirical measurements with the ATOM Matrix show a RTT of 50ms,
// so we offset the one way transmission time by half that.
constexpr Milliseconds kTransmissionOffset = 25;

}  // namespace

bool DecodeBleInnerPayload(const NetworkDeviceId& deviceIdentifier, const uint8_t* innerPayload,
                           uint8_t innerPayloadLength, int rssi, Milliseconds currentTime, NetworkMessage* outMessage,
                           Milliseconds* outReceiptTime) {
  if (innerPayloadLength > kBleMaxInnerPayloadLength) {
    jll_error("Received advertisement with unexpected length %u", innerPayloadLength);
    return false;
  }
  NetworkMessage message;
  Milliseconds originationTimeDelta;
  Milliseconds patternTimeDelta;
#if JL_IS_CONFIG(CREATURE)
  if (innerPayloadLength < kMinPayloadLength) {
    jll_error("Ignoring received creature BLE with unexpected length %u", innerPayloadLength);
    return false;
  }
  NetworkReader reader(innerPayload, innerPayloadLength);
  message.sender = deviceIdentifier;
  if (!reader.ReadNetworkDeviceId(&message.originator)) {
    jll_error("Failed to parse creature originator");
    return false;
  }
  if (!reader.ReadUint16(&message.precedence)) {
    jll_error("Failed to parse creature precedence");
    return false;
  }
  if (!reader.ReadUint8(&message.numHops)) {
    jll_error("Failed to parse creature numHops");
    return false;
  }
  uint16_t originationTimeDelta16;
  if (!reader.ReadUint16(&originationTimeDelta16)) {
    jll_error("Failed to parse creature originationTimeDelta");
    return false;
  }
  originationTimeDelta = originationTimeDelta16;
  if (!reader.ReadPatternBits(&message.currentPattern)) {
    jll_error("Failed to parse creature currentPattern");
    return false;
  }
  if (!reader.ReadPatternBits(&message.nextPattern)) {
    jll_error("Failed to parse creature nextPattern");
    return false;
  }
  uint16_t patternTimeDelta16;
  if (!reader.ReadUint16(&patternTimeDelta16)) {
    jll_error("Failed to parse creature patternTimeDelta");
    return false;
  }
  patternTimeDelta = patternTimeDelta16;
  uint8_t extensionByte = 0x00;
  if (!reader.Done()) {
    if (!reader.ReadUint8(&extensionByte)) {
      jll_error("Failed to parse creature extensionByte");
      return false;
    }
  }
  message.isCreature = (extensionByte & kExtensionByteFlagIsCreature) != 0;
  if (message.isCreature) {
    message.isPartying = (extensionByte & kExtensionByteFlagIsPartying) != 0;
    uint8_t creatureRed, creatureGreen, creatureBlue;
    if (!reader.ReadUint8(&creatureRed) || !reader.ReadUint8(&creatureGreen) || !reader.ReadUint8(&creatureBlue)) {
      jll_error("Failed to parse creature RGB");
      return false;
    }
    message.creatureColor = (creatureRed << 16) | (creatureGreen << 8) | creatureBlue;
  } else {
    message.creatureColor = 0;
    message.isPartying = false;
  }
  if ((extensionByte & kExtensionByteFlagHasOrreryScene) != 0) {
    uint8_t orrerySceneId;
    if (!reader.ReadUint8(&orrerySceneId)) {
      jll_error("Failed to parse creature orrerySceneId");
      return false;
    }
    message.orrerySceneId = orrerySceneId;
  }
  message.receiptRssi = rssi;
  message.receiptTime = currentTime;
#else   // CREATURE
  (void)rssi;
  if (innerPayloadLength < kMinPayloadLength) {
    jll_debug("Ignoring received BLE with unexpected length %u", innerPayloadLength);
    return false;
  }
  message.sender = deviceIdentifier;
  message.originator = NetworkDeviceId(&innerPayload[kOriginatorOffset]);
  message.precedence = readUint16(&innerPayload[kPrecedenceOffset]);
  message.numHops = innerPayload[kNumHopsOffset];
  originationTimeDelta = readUint16(&innerPayload[kOriginationTimeOffset]);
  message.currentPattern = readUint32(&innerPayload[kCurrentPatternOffset]);
  message.nextPattern = readUint32(&innerPayload[kNextPatternOffset]);
  patternTimeDelta = readUint16(&innerPayload[kPatternTimeOffset]);
  uint8_t extensionByte = 0;
  if (innerPayloadLength > kExtensionByteOffset) { extensionByte = innerPayload[kExtensionByteOffset]; }
  size_t orrerySceneOffset = kExtensionByteOffset + 1;
  if ((extensionByte & kExtensionByteFlagIsCreature) != 0) {
    // Skip over creature RGB.
    orrerySceneOffset += 3;
  }
  if (innerPayloadLength > orrerySceneOffset && (extensionByte & kExtensionByteFlagHasOrreryScene) != 0) {
    message.orrerySceneId = innerPayload[orrerySceneOffset];
  }
#endif  // CREATURE

  Milliseconds receiptTime;
  if (currentTime > kTransmissionOffset) {
    receiptTime = currentTime - kTransmissionOffset;
  } else {
    receiptTime = 0;
  }
  if (receiptTime >= patternTimeDelta) {
    message.currentPatternStartTime = receiptTime - patternTimeDelta;
  } else {
    message.currentPatternStartTime = 0;
  }
  if (receiptTime >= originationTimeDelta) {
    message.lastOriginationTime = receiptTime - originationTimeDelta;
  } else {
    message.lastOriginationTime = 0;
  }

  *outMessage = message;
  *outReceiptTime = receiptTime;
  return true;
}

uint8_t EncodeBleInnerPayload(const NetworkMessage& messageToSend, Milliseconds currentTime, uint8_t* innerPayload,
                              uint8_t maxInnerPayloadLength) {
  static_assert(kMaxPayloadLength <= kBleMaxInnerPayloadLength, "bad size");
  if (kMaxPayloadLength > maxInnerPayloadLength) {
    jll_error("EncodeBleInnerPayload nonsense %u > %u", kMaxPayloadLength, maxInnerPayloadLength);
    return 0;
  }
  uint8_t innerPayloadLength;

  uint16_t originationTimeDelta;
  if (messageToSend.lastOriginationTime <= currentTime && currentTime - messageToSend.lastOriginationTime <= 0xFFFF) {
    originationTimeDelta = currentTime - messageToSend.lastOriginationTime;
  } else {
    originationTimeDelta = 0xFFFF;
  }
  uint16_t patternTimeDelta;
  if (messageToSend.currentPatternStartTime <= currentTime &&
      currentTime - messageToSend.currentPatternStartTime <= 0xFFFF) {
    patternTimeDelta = currentTime - messageToSend.currentPatternStartTime;
  } else {
    patternTimeDelta = 0xFFFF;
  }

#if JL_IS_CONFIG(CREATURE)
  NetworkWriter writer(innerPayload, maxInnerPayloadLength);
  if (!writer.WriteNetworkDeviceId(messageToSend.originator)) {
    jll_error("Failed to write creature originator");
    return 0;
  }
  if (!writer.WriteUint16(messageToSend.precedence)) {
    jll_error("Failed to write creature precedence");
    return 0;
  }
  if (!writer.WriteUint8(messageToSend.numHops)) {
    jll_error("Failed to write creature numHops");
    return 0;
  }
  if (!writer.WriteUint16(originationTimeDelta)) {
    jll_error("Failed to write creature originationTimeDelta");
    return 0;
  }
  if (!writer.WriteUint32(messageToSend.currentPattern)) {
    jll_error("Failed to write creature currentPattern");
    return 0;
  }
  if (!writer.WriteUint32(messageToSend.nextPattern)) {
    jll_error("Failed to write creature nextPattern");
    return 0;
  }
  if (!writer.WriteUint16(patternTimeDelta)) {
    jll_error("Failed to write creature patternTimeDelta");
    return 0;
  }
  if (messageToSend.isCreature) {
    uint8_t extensionByte = kExtensionByteFlagIsCreature;
    if (messageToSend.isPartying) { extensionByte |= kExtensionByteFlagIsPartying; }
    if (!writer.WriteUint8(extensionByte)) {
      jll_error("Failed to write creature extensionByte");
      return 0;
    }
    uint8_t creatureRed = (messageToSend.creatureColor >> 16) & 0xFF;
    uint8_t creatureGreen = (messageToSend.creatureColor >> 8) & 0xFF;
    uint8_t creatureBlue = messageToSend.creatureColor & 0xFF;
    if (!writer.WriteUint8(creatureRed) || !writer.WriteUint8(creatureGreen) || !writer.WriteUint8(creatureBlue)) {
      jll_error("Failed to write creature RGB");
      return 0;
    }
  }
  innerPayloadLength = writer.LengthWritten();
#else   // CREATURE
  messageToSend.originator.writeTo(&innerPayload[kOriginatorOffset]);
  writeUint16(&innerPayload[kPrecedenceOffset], messageToSend.precedence);
  innerPayload[kNumHopsOffset] = messageToSend.numHops;
  writeUint16(&innerPayload[kOriginationTimeOffset], originationTimeDelta);
  writeUint32(&innerPayload[kCurrentPatternOffset], messageToSend.currentPattern);
  writeUint32(&innerPayload[kNextPatternOffset], messageToSend.nextPattern);
  writeUint16(&innerPayload[kPatternTimeOffset], patternTimeDelta);
  if (messageToSend.orrerySceneId.has_value()) {
    innerPayload[kExtensionByteOffset] = kExtensionByteFlagHasOrreryScene;
    innerPayload[kOrrerySceneOffset] = *messageToSend.orrerySceneId;
    innerPayloadLength = kOrrerySceneOffset + 1;
  } else {
    innerPayloadLength = kExtensionByteOffset;
  }
#endif  // CREATURE
  return innerPayloadLength;
}

bool FindJazzLightsAdStructure(const uint8_t* advPayload, size_t advPayloadLen, const uint8_t** innerPayload,
                               uint8_t* innerLen) {
  size_t offset = 0;
  while (offset < advPayloadLen) {
    const uint8_t structLen = advPayload[offset];
    if (structLen == 0) break;                          // Terminator/padding.
    if (offset + 1 + structLen > advPayloadLen) break;  // Malformed, truncated structure.
    const uint8_t adType = advPayload[offset + 1];
    if (adType == kBleAdvType && structLen >= 1) {
      *innerPayload = &advPayload[offset + 2];
      *innerLen = static_cast<uint8_t>(structLen - 1);
      return true;
    }
    offset += 1 + structLen;
  }
  return false;
}

}  // namespace jazzlights
