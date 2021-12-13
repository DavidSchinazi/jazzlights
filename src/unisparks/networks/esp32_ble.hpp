#ifndef UNISPARKS_NETWORKS_ESP32BLE_H
#define UNISPARKS_NETWORKS_ESP32BLE_H

#ifndef ESP32_BLE
#  define ESP32_BLE 1
#endif  // ESP32_BLE

#ifndef ESP32
#  define ESP32_BLE 0
#endif // ESP32

#if ESP32_BLE

#include <Arduino.h>
#include <BLEDevice.h>
#include <list>
#include <mutex>

#include "unisparks/util/time.hpp"

namespace unisparks {

// This class interfaces with the ESP32 Bluetooth Low Energy module. It is
// designed to allow both sending and receiving by alternating between the two.
// All calls are thread-safe.
class Esp32Ble {
public:
  using DeviceIdentifier = uint8_t[6];  // Bluetooth MAC address.
  // 29 is dictated by the BLE standard.
  static constexpr size_t kMaxInnerPayloadLength = 29;
  struct ScanResult {
    DeviceIdentifier deviceIdentifier;
    uint8_t innerPayloadLength;
    uint8_t innerPayload[kMaxInnerPayloadLength];
    int rssi;
    Milliseconds receiptTime;
  };

  // Needs to be called once during setup.
  static void Setup();

  // Needs to be called during every Arduino loop.
  static void Loop(Milliseconds currentTime);

  // Copies list of scan results since last call.
  static std::list<ScanResult> GetScanResults();

  // Configures payload to send when the next sending opportunity arises.
  // The current time, minus |timeSubtract|, will be written to the payload
  // right before sending at |timeByteOffset|.
  static void SetInnerPayload(uint8_t innerPayloadLength,
                              uint8_t* innerPayload,
                              uint8_t timeByteOffset,
                              Milliseconds timeSubtract,
                              Milliseconds currentTime);

  // Request an immediate send.
  static void TriggerSendAsap(Milliseconds currentTime);

  // Disable any future sending until SetInnerPayload is called.
  static void DisableSending(Milliseconds currentTime);

  // Extract time from a received payload.
  static Milliseconds ReadTimeFromPayload(uint8_t innerPayloadLength,
                                          const uint8_t* innerPayload,
                                          uint8_t timeByteOffset);

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
  void ReceiveAdvertisement(const DeviceIdentifier& deviceIdentifier,
                            uint8_t innerPayloadLength,
                            const uint8_t* innerPayload,
                            int rssi,
                            Milliseconds receiptTime);
  std::list<ScanResult> GetScanResultsInner();
  void SetInnerPayloadInner(uint8_t innerPayloadLength,
                            uint8_t* innerPayload,
                            uint8_t timeByteOffset,
                            Milliseconds timeSubtract,
                            Milliseconds currentTime);
  void TriggerSendAsapInner(Milliseconds currentTime);
  void DisableSendingInner(Milliseconds currentTime);
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
  static Esp32Ble* Get();

  // All these variables are protected by mutex_.
  State state_ = State::kIdle;
  bool shouldSend_ = false;
  uint8_t numUrgentSends_ = 0;
  uint8_t innerPayloadLength_ = 0;
  uint8_t innerPayload_[kMaxInnerPayloadLength];
  uint8_t timeByteOffset_ = kMaxInnerPayloadLength;
  Milliseconds timeSubtract_ = 0;
  std::list<ScanResult> scanResults_;
  Milliseconds timeToStopAdvertising_ = 0;
  Milliseconds timeToStopScanning_ = 0;
  std::mutex mutex_;
};

}  // namespace unisparks

#endif // ESP32_BLE
#endif // UNISPARKS_NETWORKS_ESP32BLE_H
