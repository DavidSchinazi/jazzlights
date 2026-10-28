#ifndef JL_NETWORK_ESP32_BLE_H
#define JL_NETWORK_ESP32_BLE_H

#ifdef ESP32

#include "jazzlights/util/config.h"

#if !JL_DISABLE_BLUETOOTH && !JL_ESP32_NIMBLE

#include <esp_gap_ble_api.h>

#include "jazzlights/network/esp32_ble_common.h"

namespace jazzlights {

// This class interfaces with the ESP32 Bluetooth Low Energy module using the Bluedroid stack. It is
// designed to allow both sending and receiving by alternating between the two.
// All calls are thread-safe.
class Esp32BleNetwork : public Esp32BleCommonNetwork {
 public:
  static Esp32BleNetwork* Get();

 protected:
  void MaybeUpdateAdvertisingState() override;

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

  explicit Esp32BleNetwork() : Esp32BleCommonNetwork(InitBluetoothStackAndQueryLocalDeviceId()) {}
  void StartScanning();
  void StopScanning();
  void StartAdvertising();
  void StopAdvertising();
  void StartConfigureAdvertising();
  void UpdateState(State expectedCurrentState, State newState);
  void GapCallbackInner(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param, Microseconds callbackTime);

  static void GapCallback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param);

  static NetworkDeviceId InitBluetoothStackAndQueryLocalDeviceId();

  State state_ = State::kIdle;  // Protected by mutex_.
};

}  // namespace jazzlights

#endif  // !JL_DISABLE_BLUETOOTH && !JL_ESP32_NIMBLE
#endif  // ESP32
#endif  // JL_NETWORK_ESP32_BLE_H
