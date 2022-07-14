#ifndef UNISPARKS_NETWORKS_ESP32BLE_H
#define UNISPARKS_NETWORKS_ESP32BLE_H

#ifndef ESP32_BLE
#  ifdef ESP32
#    define ESP32_BLE 1
#  else // ESP32
#    define ESP32_BLE 0
#  endif // ESP32
#endif  // ESP32_BLE

#if ESP32_BLE

#include <Arduino.h>
#include <BLEDevice.h>
#include <list>
#include <mutex>

#include "unisparks/util/time.hpp"
#include "unisparks/network.hpp"

namespace unisparks {

// This class interfaces with the ESP32 Bluetooth Low Energy module. It is
// designed to allow both sending and receiving by alternating between the two.
// All calls are thread-safe.
class Esp32BleNetwork : public Network {
 public:
  static Esp32BleNetwork* get();

  void setMessageToSend(const NetworkMessage& messageToSend,
                        Milliseconds currentTime) override;
  void disableSending(Milliseconds currentTime) override;
  void triggerSendAsap(Milliseconds currentTime) override;

  // Get this device's BLE MAC address.
  NetworkDeviceId getLocalDeviceId() override {
    return localDeviceId_;
  }
  const char* networkName() const override {
    return "ESP32BLE";
  }
  bool shouldEcho() const override { return true; }
 protected:
  void runLoopImpl(Milliseconds currentTime) override;
  NetworkStatus update(NetworkStatus status, Milliseconds currentTime) override;
  std::list<NetworkMessage> getReceivedMessagesImpl(Milliseconds currentTime) override;

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

  explicit Esp32BleNetwork();
  void StartScanning(Milliseconds currentTime);
  void StopScanning(Milliseconds currentTime);
  void StartAdvertising(Milliseconds currentTime);
  void StopAdvertising(Milliseconds currentTime);
  void StartConfigureAdvertising(Milliseconds currentTime);
  void MaybeUpdateAdvertisingState(Milliseconds currentTime);
  void StopAdvertisingIn(Milliseconds duration);
  void StopScanningIn(Milliseconds duration);
  void ReceiveAdvertisement(const NetworkDeviceId& deviceIdentifier,
                            uint8_t innerPayloadLength,
                            const uint8_t* innerPayload,
                            int rssi,
                            Milliseconds currentTime);
  uint8_t GetNextInnerPayloadToSend(uint8_t* innerPayload,
                                    uint8_t maxInnerPayloadLength,
                                    Milliseconds currentTime);
  void UpdateState(State expectedCurrentState, State newState);
  bool ExtractShouldTriggerSendAsap();
  void GapCallbackInner(esp_gap_ble_cb_event_t event,
                        esp_ble_gap_cb_param_t *param,
                        Milliseconds currentTime);
                    
  static void GapCallback(esp_gap_ble_cb_event_t event,
                          esp_ble_gap_cb_param_t *param);

  NetworkDeviceId localDeviceId_;
  std::mutex mutex_;
  // All the variables below are protected by mutex_.
  State state_ = State::kIdle;
  bool isSendingEnabled_ = false;
  bool hasDataToSend_ = false;
  NetworkMessage messageToSend_;
  uint8_t numUrgentSends_ = 0;
  std::list<NetworkMessage> receivedMessages_;
  Milliseconds timeToStopAdvertising_ = 0;
  Milliseconds timeToStopScanning_ = 0;
};

}  // namespace unisparks

#endif // ESP32_BLE
#endif // UNISPARKS_NETWORKS_ESP32BLE_H
