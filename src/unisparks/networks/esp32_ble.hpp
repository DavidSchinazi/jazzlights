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
class Esp32Ble {
public:
  static Esp32Ble* Get();

  // Needs to be called once during setup.
  void setup();

  // Needs to be called during every Arduino loop.
  void runLoop(Milliseconds currentTime);

  void setMessageToSend(const NetworkMessage& messageToSend,
                        Milliseconds currentTime);

  // Copies list of scan results since last call.
  std::list<NetworkMessage> getReceivedMessages(Milliseconds currentTime);

  // Request an immediate send.
  void triggerSendAsap(Milliseconds currentTime);

  // Disable any future sending until setMessageToSend is called.
  static void DisableSending(Milliseconds currentTime);

  // Extract time from a received payload.
  static Milliseconds ReadTimeFromPayload(uint8_t innerPayloadLength,
                                          const uint8_t* innerPayload,
                                          uint8_t timeByteOffset);

  // Get this device's BLE MAC address.
  static void GetLocalAddress(NetworkDeviceId* localAddress);

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

  Esp32Ble();
  void Init();
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
                            Milliseconds receiptTime);
  void DisableSendingInner(Milliseconds currentTime);
  uint8_t GetNextInnerPayloadToSend(uint8_t* innerPayload,
                                    uint8_t maxInnerPayloadLength,
                                    Milliseconds currentTime);
  void UpdateState(State expectedCurrentState, State newState);
  bool ExtractShouldTriggerSendAsap();
  void GetLocalAddressInner(NetworkDeviceId* localAddress);
  void GapCallbackInner(esp_gap_ble_cb_event_t event,
                        esp_ble_gap_cb_param_t *param,
                        Milliseconds currentTime);
                    
  static void GapCallback(esp_gap_ble_cb_event_t event,
                          esp_ble_gap_cb_param_t *param);

  // All these variables are protected by mutex_.
  State state_ = State::kIdle;
  bool shouldSend_ = false;
  uint8_t numUrgentSends_ = 0;
  uint8_t innerPayloadLength_ = 0;
  uint8_t innerPayload_[kMaxInnerPayloadLength];
  uint8_t timeByteOffset_ = kMaxInnerPayloadLength;
  Milliseconds timeSubtract_ = 0;
  std::list<NetworkMessage> receivedMessages_;
  Milliseconds timeToStopAdvertising_ = 0;
  Milliseconds timeToStopScanning_ = 0;
  NetworkDeviceId localDeviceIdentifier_;
  std::mutex mutex_;
};

}  // namespace unisparks

#endif // ESP32_BLE
#endif // UNISPARKS_NETWORKS_ESP32BLE_H
