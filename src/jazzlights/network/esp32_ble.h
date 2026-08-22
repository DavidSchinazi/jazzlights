#ifndef JL_NETWORK_ESP32_BLE_H
#define JL_NETWORK_ESP32_BLE_H

#ifdef ESP32

#ifndef JL_DISABLE_BLUETOOTH
#define JL_DISABLE_BLUETOOTH 0
#endif  // JL_DISABLE_BLUETOOTH

#include "jazzlights/network/network.h"

#if !JL_DISABLE_BLUETOOTH

#include <esp_gap_ble_api.h>

#include <atomic>
#include <list>
#include <mutex>

namespace jazzlights {

// This class interfaces with the ESP32 Bluetooth Low Energy module. It is
// designed to allow both sending and receiving by alternating between the two.
// All calls are thread-safe.
class Esp32BleNetwork : public Network {
 public:
  static Esp32BleNetwork* get();

  void setMessageToSend(const NetworkMessage& messageToSend) override;
  void disableSending() override;
  void triggerSendAsap() override;

  // Get this device's BLE MAC address.
  NetworkDeviceId getLocalDeviceId() const override { return localDeviceId_; }
  NetworkType type() const override { return NetworkType::kBLE; }
  bool shouldEcho() const override { return true; }
  OptionalMicroseconds getLastReceiveTime() const override { return lastReceiveTime_.load(std::memory_order_relaxed); }
  std::string getStatusStr() override;

 protected:
  void runLoopImpl() override;
  NetworkStatus update(NetworkStatus /*status*/) override { return CONNECTED; }
  std::list<NetworkMessage> getReceivedMessagesImpl() override;

 private:
  // All public calls in this class are static, but internally they are backed by a
  // singleton which keeps state and uses a mutex to allow safe access from callers
  // and the internal BLE thread.
  enum class State {
    kInvalid,
    kIdle,
    kStartingScan,
    kScanning,
    kStoppingScan,
    kConfiguringAdvertising,
    kStartingAdvertising,
    kAdvertising,
    kStoppingAdvertising,
  };
  std::string StateToString(State state);
  // 29 is dictated by the BLE standard.
  static constexpr size_t kMaxInnerPayloadLength = 29;

  explicit Esp32BleNetwork() {}
  void StartScanning();
  void StopScanning();
  void StartAdvertising();
  void StopAdvertising();
  void StartConfigureAdvertising();
  void MaybeUpdateAdvertisingState();
  void StopAdvertisingIn(Microseconds duration);
  void StopScanningIn(Microseconds duration);
  void ReceiveAdvertisement(const NetworkDeviceId& deviceIdentifier, uint8_t innerPayloadLength,
                            const uint8_t* innerPayload, int rssi, Microseconds callbackTime);
  size_t GetNextInnerPayloadToSend(uint8_t* innerPayload, uint8_t maxInnerPayloadLength);
  void UpdateState(State expectedCurrentState, State newState);
  bool ExtractShouldTriggerSendAsap();
  void GapCallbackInner(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param, Microseconds callbackTime);

  static void GapCallback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param);

  static NetworkDeviceId InitBluetoothStackAndQueryLocalDeviceId();

  const NetworkDeviceId localDeviceId_ = InitBluetoothStackAndQueryLocalDeviceId();
  std::atomic<OptionalMicroseconds> lastReceiveTime_;
  std::mutex mutex_;
  // All the variables below are protected by mutex_.
  State state_ = State::kIdle;
  bool hasDataToSend_ = false;
  NetworkMessage messageToSend_;
  uint8_t numUrgentSends_ = 0;
  std::list<NetworkMessage> receivedMessages_;
  OptionalMicroseconds timeToStopAdvertising_;
  OptionalMicroseconds timeToStopScanning_;
};

}  // namespace jazzlights
#else   // JL_DISABLE_BLUETOOTH

namespace jazzlights {
// This version of Esp32BleNetwork is a no-op designed to allow disabling all Bluetooth support without having to modify
// the rest of the codebase.
class Esp32BleNetwork : public Network {
 public:
  static Esp32BleNetwork* get();

  void setMessageToSend(const NetworkMessage& /*messageToSend*/) override {}
  void disableSending() override {}
  void triggerSendAsap() override {}
  NetworkDeviceId getLocalDeviceId() const override { return NetworkDeviceId(); }
  NetworkType type() const override { return NetworkType::kBLE; }
  bool shouldEcho() const override { return false; }
  OptionalMicroseconds getLastReceiveTime() const override { return std::nullopt; }
  std::string getStatusStr() override { return "Compiled Out"; }

 protected:
  void runLoopImpl() override {}
  NetworkStatus update(NetworkStatus /*status*/) override { return CONNECTED; }
  std::list<NetworkMessage> getReceivedMessagesImpl() override { return {}; }
};
}  // namespace jazzlights
#endif  // JL_DISABLE_BLUETOOTH
#endif  // ESP32
#endif  // JL_NETWORK_ESP32_BLE_H
