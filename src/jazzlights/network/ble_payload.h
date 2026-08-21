#ifndef JL_NETWORK_BLE_PAYLOAD_H
#define JL_NETWORK_BLE_PAYLOAD_H

#include <cstddef>
#include <cstdint>

#include "jazzlights/network/network.h"

namespace jazzlights {

// Squats on an unused Bluetooth Advertising Data Type.
// https://bitbucket.org/bluetooth-SIG/public/src/main/assigned_numbers/core/ad_types.yaml
// https://www.bluetooth.com/specifications/assigned-numbers/
constexpr uint8_t kBleAdvType = 0x96;

// 29 is dictated by the BLE standard (legacy advertising payload hard limit).
constexpr size_t kBleMaxInnerPayloadLength = 29;

// Encodes messageToSend into innerPayload (caller-owned buffer of at least
// maxInnerPayloadLength bytes). currentTime is passed in explicitly (rather than reading a clock
// internally) purely so this function is unit-testable. Returns the number of bytes written, or 0
// if the payload doesn't fit within maxInnerPayloadLength.
uint8_t EncodeBleInnerPayload(const NetworkMessage& messageToSend, Milliseconds currentTime, uint8_t* innerPayload,
                              uint8_t maxInnerPayloadLength);

// Decodes innerPayload (the bytes after a raw AD structure's [len][kBleAdvType] header) into
// *outMessage, with *outReceiptTime set to the reconstructed local receipt time. deviceIdentifier,
// rssi (CREATURE builds only), and currentTime are supplied by the caller. Returns false (leaving
// *outMessage/*outReceiptTime untouched) if the payload is malformed or too short.
bool DecodeBleInnerPayload(const NetworkDeviceId& deviceIdentifier, const uint8_t* innerPayload,
                           uint8_t innerPayloadLength, int rssi, Milliseconds currentTime, NetworkMessage* outMessage,
                           Milliseconds* outReceiptTime);

// Walks a raw advertisement payload (the full concatenated set of BLE AD structures -- e.g. as
// returned by NimBLE's NimBLEAdvertisedDevice::getPayload(), which is NOT pre-split by AD type)
// looking for the one AD structure whose type byte is kBleAdvType. On success, *innerPayload
// points INTO advPayload (no copy) and *innerLen is set to that structure's payload length.
// Returns false if no matching AD structure is found or the data is malformed/truncated.
bool FindJazzLightsAdStructure(const uint8_t* advPayload, size_t advPayloadLen, const uint8_t** innerPayload,
                               uint8_t* innerLen);

}  // namespace jazzlights

#endif  // JL_NETWORK_BLE_PAYLOAD_H
