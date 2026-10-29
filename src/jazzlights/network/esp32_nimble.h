#ifndef JL_NETWORK_ESP32_NIMBLE_H
#define JL_NETWORK_ESP32_NIMBLE_H

#ifdef ESP32

#include "jazzlights/util/config.h"

#if !JL_DISABLE_BLUETOOTH && JL_ESP32_NIMBLE

#include <sdkconfig.h>

// The NimBLE headers live at different paths in ESP-IDF and in NimBLE-Arduino.
#if CONFIG_BT_NIMBLE_ENABLED
// NimBLE was enabled in sdkconfig so we're building with ESP-IDF and its NimBLE component.
#define JL_NIMBLE_ARDUINO 0
#include <host/ble_hs.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#elif CONFIG_BT_BLUEDROID_ENABLED
// The prebuilt ESP-IDF libraries from the Arduino core have Bluedroid enabled, so we use NimBLE-Arduino.
#define JL_NIMBLE_ARDUINO 1
#include <nimconfig.h>
// nimconfig.h must be included first.
#include <nimble/esp_port/esp-hci/include/esp_nimble_hci.h>
#include <nimble/nimble/host/include/host/ble_hs.h>
#include <nimble/porting/nimble/include/nimble/nimble_port.h>
#include <nimble/porting/npl/freertos/include/nimble/nimble_port_freertos.h>
#else
#error "JL_ESP32_NIMBLE requires Bluetooth to be enabled in sdkconfig, see sdkconfig.nimble.defaults"
#endif

#include <atomic>
#include <mutex>

#include "jazzlights/network/esp32_ble_common.h"

namespace jazzlights {

// This class interfaces with the ESP32 Bluetooth Low Energy module using the NimBLE stack. It is
// designed to allow both sending and receiving by alternating between the two. It behaves
// identically to Esp32BleNetwork but results in a significantly smaller binary.
// When built with `framework = espidf, arduino` it uses the NimBLE stack from ESP-IDF, which needs
// to be enabled in sdkconfig. When built with `framework = arduino` it uses the NimBLE-Arduino library.
// All calls are thread-safe.
class Esp32NimBleNetwork : public Esp32BleCommonNetwork {
 public:
  static Esp32NimBleNetwork* Get();

 protected:
  void MaybeUpdateAdvertisingState() override;

 private:
  // Unlike with Bluedroid, NimBLE calls to start and stop scanning or advertising are synchronous,
  // so we don't need intermediate states.
  enum class State {
    kIdle,
    kScanning,
    kAdvertising,
  };
  std::string StateToString(State state);

  explicit Esp32NimBleNetwork() : Esp32BleCommonNetwork(InitBluetoothStackAndQueryLocalDeviceId()) {}
  // The functions below must be called with radioMutex_ held.
  void StartScanning();
  void StopScanning();
  void StartAdvertising();
  void StopAdvertising();

  void HandleGapEvent(struct ble_gap_event* event, Microseconds callbackTime);

  static int GapEventCallback(struct ble_gap_event* event, void* arg);
  static void HostTask(void* parameters);
  static void HostSyncCallback();
  static void HostResetCallback(int reason);

  static NetworkDeviceId InitBluetoothStackAndQueryLocalDeviceId();

  // Set when the NimBLE host resets, which cancels any ongoing scanning or advertising. This is static because the host
  // could theoretically reset before our constructor returns.
  static std::atomic<bool> sHostResetPending_;

  // Serializes changes to the radio state. MaybeUpdateAdvertisingState() holds radioMutex_ for its entire duration
  // and takes the parent class's mutex_ inside it, so the lock order is always radioMutex_ then mutex_. Code that holds
  // mutex_ must never acquire radioMutex_, otherwise we could deadlock. This is why GapEventCallback(), which takes
  // mutex_ via ReceiveAdvertisement(), never acquires radioMutex_. Note that the parent class's TriggerSendAsap()
  // releases mutex_ before calling MaybeUpdateAdvertisingState().
  std::mutex radioMutex_;
  State state_ = State::kIdle;  // Protected by radioMutex_.
};

}  // namespace jazzlights

#endif  // !JL_DISABLE_BLUETOOTH && JL_ESP32_NIMBLE
#endif  // ESP32
#endif  // JL_NETWORK_ESP32_NIMBLE_H
