#ifndef JL_NETWORK_ESP32_BLE_COMMON_H
#define JL_NETWORK_ESP32_BLE_COMMON_H

#include "jazzlights/util/config.h"

#ifdef ESP32

#include "jazzlights/network/network.h"

#if !JL_DISABLE_BLUETOOTH

#include <atomic>
#include <list>
#include <mutex>

#include "jazzlights/util/log.h"

#ifndef ESP32_BLE_DEBUG_OVERRIDE
#define ESP32_BLE_DEBUG_OVERRIDE 0
#endif  // ESP32_BLE_DEBUG_OVERRIDE

#if ESP32_BLE_DEBUG_OVERRIDE
#define ESP32_BLE_DEBUG(...) jll_info(__VA_ARGS__)
#define ESP32_BLE_DEBUG_ENABLED() 1
#else  // ESP32_BLE_DEBUG_OVERRIDE
#define ESP32_BLE_DEBUG(...) jll_debug(__VA_ARGS__)
#define ESP32_BLE_DEBUG_ENABLED() IsDebugLoggingEnabled()
#endif  // ESP32_BLE_DEBUG_OVERRIDE

namespace jazzlights {

// Base class for our ESP32 Bluetooth Low Energy networks. It holds everything that does not depend on which Bluetooth
// stack is in use: the message to send, received messages, and the advertisement payload format. Subclasses implement
// the state machine that alternates between scanning and advertising.
// All calls are thread-safe.
class Esp32BleCommonNetwork : public Network {
 public:
  void SetMessageToSend(const ProtocolMessage& messageToSend) override;
  void DisableSending() override;
  void TriggerSendAsap() override;

  // Get this device's BLE MAC address.
  NetworkDeviceId GetLocalDeviceId() const override { return localDeviceId_; }
  NetworkType Type() const override { return NetworkType::kBLE; }
  bool ShouldEcho() const override { return true; }
  OptionalMicroseconds GetLastReceiveTime() const override { return lastReceiveTime_.load(std::memory_order_relaxed); }
  std::string GetStatusStr() override;

 protected:
  // Squat on an unused Bluetooth Advertising Data Type.
  // https://bitbucket.org/bluetooth-SIG/public/src/main/assigned_numbers/core/adTypes.yaml
  // https://www.bluetooth.com/specifications/assigned-numbers/
  static constexpr uint8_t kAdvType = 0x96;
  // 29 is dictated by the BLE standard.
  static inline constexpr size_t kMaxInnerPayloadLength = 29;
  static inline constexpr size_t kMaxAdvPayloadLength = 2 + kMaxInnerPayloadLength;
  static inline constexpr size_t kMaxAdvDataHexStringSize = kMaxAdvPayloadLength * 2 + 1;

  explicit Esp32BleCommonNetwork(const NetworkDeviceId& localDeviceId) : localDeviceId_(localDeviceId) {}

  void RunLoopImpl() override;
  NetworkStatus Update(NetworkStatus /*status*/) override { return kConnected; }
  std::list<ProtocolMessage> GetReceivedMessagesImpl() override;

  // Starts or stops scanning and advertising as needed. Called from RunLoop() and TriggerSendAsap().
  virtual void MaybeUpdateAdvertisingState() = 0;

  void StopAdvertisingIn(Microseconds duration);
  void StopScanningIn(Microseconds duration);
  void ReceiveAdvertisement(const NetworkDeviceId& deviceIdentifier, uint8_t innerPayloadLength,
                            const uint8_t* innerPayload, int rssi, Microseconds callbackTime);
  // Fills advPayload with the next advertisement to send, and returns its length.
  size_t GetNextAdvPayloadToSend(uint8_t advPayload[kMaxAdvPayloadLength]);
  bool ExtractShouldTriggerSendAsap();

  static void ConvertToHex(char* target, size_t targetLength, const uint8_t* source, size_t sourceLength);

  // Subclasses may also use mutex_ to protect their own state.
  std::mutex mutex_;
  bool hasDataToSend_ = false;                  // Protected by mutex_.
  uint8_t numUrgentSends_ = 0;                  // Protected by mutex_.
  OptionalMicroseconds timeToStopAdvertising_;  // Protected by mutex_.
  OptionalMicroseconds timeToStopScanning_;     // Protected by mutex_.

 private:
  size_t GetNextInnerPayloadToSend(uint8_t* innerPayload, uint8_t maxInnerPayloadLength);

  const NetworkDeviceId localDeviceId_;
  std::atomic<OptionalMicroseconds> lastReceiveTime_;
  ProtocolMessage messageToSend_;                // Protected by mutex_.
  std::list<ProtocolMessage> receivedMessages_;  // Protected by mutex_.
};

}  // namespace jazzlights

#else  // JL_DISABLE_BLUETOOTH

namespace jazzlights {

// This is a no-op network designed to allow disabling all Bluetooth support without having to modify the rest of the
// codebase.
class Esp32BleNoOpNetwork : public Network {
 public:
  static Esp32BleNoOpNetwork* Get();

  void SetMessageToSend(const ProtocolMessage& /*messageToSend*/) override {}
  void DisableSending() override {}
  void TriggerSendAsap() override {}
  NetworkDeviceId GetLocalDeviceId() const override { return NetworkDeviceId(); }
  NetworkType Type() const override { return NetworkType::kBLE; }
  bool ShouldEcho() const override { return false; }
  OptionalMicroseconds GetLastReceiveTime() const override { return std::nullopt; }
  std::string GetStatusStr() override { return "Compiled Out"; }

 protected:
  void RunLoopImpl() override {}
  NetworkStatus Update(NetworkStatus /*status*/) override { return kConnected; }
  std::list<ProtocolMessage> GetReceivedMessagesImpl() override { return {}; }
};

}  // namespace jazzlights

#endif  // JL_DISABLE_BLUETOOTH
#endif  // ESP32
#endif  // JL_NETWORK_ESP32_BLE_COMMON_H
