#ifndef JL_NETWORK_ESP32_BLE_H
#define JL_NETWORK_ESP32_BLE_H

#ifdef ESP32

#ifndef JL_DISABLE_BLUETOOTH
#define JL_DISABLE_BLUETOOTH 0
#endif  // JL_DISABLE_BLUETOOTH

#include "jazzlights/network/network.h"

#if !JL_DISABLE_BLUETOOTH

#include <NimBLEDevice.h>

#include <atomic>
#include <list>
#include <mutex>

namespace jazzlights {

// This class interfaces with the ESP32 Bluetooth Low Energy module via NimBLE-Arduino. It is
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
  Milliseconds getLastReceiveTime() const override { return lastReceiveTime_; }
  std::string getStatusStr() override;

 protected:
  void runLoopImpl() override;
  NetworkStatus update(NetworkStatus /*status*/) override { return CONNECTED; }
  std::list<NetworkMessage> getReceivedMessagesImpl() override;

 private:
  // All public calls in this class are static, but internally they are backed by a
  // singleton which keeps state and uses a mutex to allow safe access from callers
  // and NimBLE's internal host task.
  //
  // Unlike the previous raw-Bluedroid implementation, NimBLE-Arduino's start()/stop() calls are
  // synchronous, so there's no need to track "requested" vs. "completed" sub-states -- the device
  // is always in exactly one of these two states.
  enum class State {
    kScanning,
    kAdvertising,
  };

  class ScanCallbacks : public NimBLEScanCallbacks {
   public:
    explicit ScanCallbacks(Esp32BleNetwork* owner) : owner_(owner) {}
    void onResult(const NimBLEAdvertisedDevice* dev) override;

   private:
    Esp32BleNetwork* owner_;
  };

  explicit Esp32BleNetwork();
  void StartScanning();
  void StartAdvertising();
  void MaybeUpdateAdvertisingState();
  void ReceiveAdvertisement(const NetworkDeviceId& deviceIdentifier, uint8_t innerPayloadLength,
                            const uint8_t* innerPayload, int rssi);
  uint8_t GetNextInnerPayloadToSend(uint8_t* innerPayload, uint8_t maxInnerPayloadLength);
  bool ExtractShouldTriggerSendAsap();

  // Only initializes the NimBLE stack and queries the local device ID -- run as localDeviceId_'s
  // member-initializer, before any other member (including scan_/advertising_/scanCallbacks_) has
  // been constructed, so it must not touch instance state. The rest of the setup (scan/advertising
  // configuration, starting the first scan) happens in the constructor body below instead.
  static NetworkDeviceId InitBluetoothStackAndQueryLocalDeviceId();

  const NetworkDeviceId localDeviceId_ = InitBluetoothStackAndQueryLocalDeviceId();
  std::atomic<Milliseconds> lastReceiveTime_{-1};
  std::mutex mutex_;
  // All the variables below are protected by mutex_.
  State state_ = State::kScanning;
  bool hasDataToSend_ = false;
  NetworkMessage messageToSend_;
  uint8_t numUrgentSends_ = 0;
  std::list<NetworkMessage> receivedMessages_;
  Milliseconds timeToStopAdvertising_ = 0;
  Milliseconds timeToStopScanning_ = 0;

  NimBLEScan* scan_ = nullptr;
  NimBLEAdvertising* advertising_ = nullptr;
  ScanCallbacks scanCallbacks_{this};
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
  Milliseconds getLastReceiveTime() const override { return -1; }
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
