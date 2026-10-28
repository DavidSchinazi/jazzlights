#include "jazzlights/network/esp32_ble_common.h"

#ifdef ESP32
#if !JL_DISABLE_BLUETOOTH

#include <cstring>
#include <limits>
#include <optional>
#include <string>

#include "jazzlights/protocol/wire.h"

namespace jazzlights {

// static
void Esp32BleCommonNetwork::ConvertToHex(char* target, size_t targetLength, const uint8_t* source,
                                         size_t sourceLength) {
  if (targetLength == 0) { return; }
  if (targetLength <= sourceLength * 2) {
    target[0] = '\0';
    return;
  }
  for (size_t i = 0; i < sourceLength; i++) { sprintf(&target[2 * i], "%.2x", static_cast<char>(source[i])); }
  target[sourceLength * 2] = '\0';
}

void Esp32BleCommonNetwork::StopAdvertisingIn(Microseconds duration) {
  const std::lock_guard<std::mutex> lock(mutex_);
  timeToStopAdvertising_ = TimeMicros() + duration;
}

void Esp32BleCommonNetwork::StopScanningIn(Microseconds duration) {
  const std::lock_guard<std::mutex> lock(mutex_);
  timeToStopScanning_ = TimeMicros() + duration;
}

std::list<ProtocolMessage> Esp32BleCommonNetwork::GetReceivedMessagesImpl() {
  std::list<ProtocolMessage> results;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    results = std::move(receivedMessages_);
    receivedMessages_.clear();
  }
  return results;
}

void Esp32BleCommonNetwork::TriggerSendAsap() {
  ESP32_BLE_DEBUG("TriggerSendAsap");
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    numUrgentSends_ = 10;
  }
  MaybeUpdateAdvertisingState();
}

void Esp32BleCommonNetwork::SetMessageToSend(const ProtocolMessage& messageToSend) {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (!hasDataToSend_ || !messageToSend_.IsEqualExceptOriginationTime(messageToSend)) {
    ESP32_BLE_DEBUG("Setting messageToSend %s", NetworkMessageToString(messageToSend).c_str());
    ESP32_BLE_DEBUG("Old messageToSend was %s", NetworkMessageToString(messageToSend_).c_str());
  }
  hasDataToSend_ = true;
  messageToSend_ = messageToSend;
}

void Esp32BleCommonNetwork::DisableSending() {
  const std::lock_guard<std::mutex> lock(mutex_);
  hasDataToSend_ = false;
}

void Esp32BleCommonNetwork::ReceiveAdvertisement(const NetworkDeviceId& deviceIdentifier, uint8_t innerPayloadLength,
                                                 const uint8_t* innerPayload, int rssi, Microseconds callbackTime) {
  if (innerPayloadLength > kMaxInnerPayloadLength) {
    jll_error("Received advertisement with unexpected length %u", innerPayloadLength);
    return;
  }
  // Empirical measurements with the ATOM Matrix show a RTT of 50ms,
  // so we offset the one way transmission time by half that.
  static constexpr Microseconds kTransmissionOffset = 25 * kMicrosecondsPerMillisecond;
  const Microseconds receiptTime = callbackTime - kTransmissionOffset;
  std::optional<ProtocolMessage> parsedMessage =
      ParseProtocolMessage(innerPayload, innerPayloadLength, /*isBle=*/true, receiptTime);
  if (!parsedMessage) { return; }
  ProtocolMessage& message = *parsedMessage;
#if JL_IS_CONFIG(CREATURE)
  message.receiptRssi = rssi;
  message.receiptTime = receiptTime;
#else   // CREATURE
  (void)rssi;
#endif  // CREATURE
  message.sender = deviceIdentifier;

  ESP32_BLE_DEBUG("Received %s", NetworkMessageToString(message).c_str());
  lastReceiveTime_.store(receiptTime, std::memory_order_relaxed);

  {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (receivedMessages_.size() > 100) {
      // Make sure we do not run out of memory if no one is
      // periodically calling GetScanResults().
      receivedMessages_.clear();
    }
    receivedMessages_.push_back(message);
  }
}

size_t Esp32BleCommonNetwork::GetNextInnerPayloadToSend(uint8_t* innerPayload, uint8_t maxInnerPayloadLength) {
  const std::lock_guard<std::mutex> lock(mutex_);
  static_assert(kMaxBleProtocolPayloadLength <= kMaxInnerPayloadLength, "bad size");
  if (kMaxBleProtocolPayloadLength > maxInnerPayloadLength) {
    jll_error("GetNextInnerPayloadToSend nonsense %zu > %u", kMaxBleProtocolPayloadLength, maxInnerPayloadLength);
    return 0;
  }

  const size_t innerPayloadLength =
      WriteProtocolMessage(messageToSend_, /*isBle=*/true, innerPayload, maxInnerPayloadLength);
  if (innerPayloadLength == 0) { return 0; }
  if (ESP32_BLE_DEBUG_ENABLED()) {
    char advRawData[kMaxAdvDataHexStringSize] = {};
    ConvertToHex(advRawData, sizeof(advRawData), innerPayload, innerPayloadLength);
    ESP32_BLE_DEBUG("Setting inner payload to <%zu:%s>", innerPayloadLength, advRawData);
  }
  return innerPayloadLength;
}

size_t Esp32BleCommonNetwork::GetNextAdvPayloadToSend(uint8_t advPayload[kMaxAdvPayloadLength]) {
  size_t innerPayloadSize = GetNextInnerPayloadToSend(&advPayload[2], kMaxInnerPayloadLength);
  if (innerPayloadSize > kMaxInnerPayloadLength) {
    jll_error("getNextAdvertisementToSend returned nonsense %zu", innerPayloadSize);
    innerPayloadSize = kMaxInnerPayloadLength;
    memset(advPayload, 0, kMaxAdvPayloadLength);
  }
  static_assert(kMaxInnerPayloadLength <= static_cast<size_t>(std::numeric_limits<uint8_t>::max() - 1), "bad size");
  advPayload[0] = 1 + innerPayloadSize;
  advPayload[1] = kAdvType;
  if (ESP32_BLE_DEBUG_ENABLED()) {
    char advRawData[kMaxAdvDataHexStringSize] = {};
    ConvertToHex(advRawData, sizeof(advRawData), advPayload, 2 + innerPayloadSize);
    ESP32_BLE_DEBUG("Sending adv<%zu:%s>", 2 + innerPayloadSize, advRawData);
  }
  return 2 + innerPayloadSize;
}

bool Esp32BleCommonNetwork::ExtractShouldTriggerSendAsap() {
  bool shouldTriggerSendAsap;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (numUrgentSends_ > 0) {
      numUrgentSends_--;
      shouldTriggerSendAsap = true;
    } else {
      shouldTriggerSendAsap = false;
    }
  }
  return shouldTriggerSendAsap;
}

void Esp32BleCommonNetwork::RunLoopImpl() { MaybeUpdateAdvertisingState(); }

std::string Esp32BleCommonNetwork::GetStatusStr() {
  char statStr[100] = {};
  const OptionalMicroseconds lastRcv = GetLastReceiveTime();
  snprintf(statStr, sizeof(statStr) - 1, "%lldms", lastRcv ? MsSinceForLogs(*lastRcv) : -1);
  return std::string(statStr);
}

}  // namespace jazzlights

#else  // JL_DISABLE_BLUETOOTH

namespace jazzlights {

// static
Esp32BleNoOpNetwork* Esp32BleNoOpNetwork::Get() {
  static Esp32BleNoOpNetwork sStaticInstance;
  return &sStaticInstance;
}

}  // namespace jazzlights

#endif  // JL_DISABLE_BLUETOOTH
#endif  // ESP32
