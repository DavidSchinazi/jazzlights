#ifndef JL_NETWORKS_ARDUINO_ESP32_WIFI_H
#define JL_NETWORKS_ARDUINO_ESP32_WIFI_H

#ifdef ESP32

#include "jazzlights/network.h"

// ArduinoEsp32WiFiNetwork is currently work-in-progress.
// TODO: finish this and use it to replace ArduinoEspWiFiNetwork.
#define JL_ARDUINO_ESP32_WIFI 0

#ifndef JL_ARDUINO_ESP32_WIFI
#define JL_ARDUINO_ESP32_WIFI 1
#endif  // JL_ARDUINO_ESP32_WIFI

#if JL_ARDUINO_ESP32_WIFI

#include <WiFi.h>

#include <atomic>
#include <mutex>

namespace jazzlights {

class ArduinoEsp32WiFiNetwork : public Network {
 public:
  static ArduinoEsp32WiFiNetwork* get();

  NetworkStatus update(NetworkStatus status, Milliseconds currentTime) override;
  NetworkDeviceId getLocalDeviceId() override { return localDeviceId_; }
  const char* networkName() const override { return "ArduinoEsp32WiFi"; }
  const char* shortNetworkName() const override { return "WiFi"; }
  std::string getStatusStr(Milliseconds currentTime) override;
  void setMessageToSend(const NetworkMessage& messageToSend, Milliseconds currentTime) override;
  void disableSending(Milliseconds currentTime) override;
  void triggerSendAsap(Milliseconds currentTime) override;
  bool shouldEcho() const override { return false; }
  Milliseconds getLastReceiveTime() const override { return lastReceiveTime_.load(std::memory_order_relaxed); }

 protected:
  std::list<NetworkMessage> getReceivedMessagesImpl(Milliseconds currentTime) override;
  void runLoopImpl(Milliseconds /*currentTime*/) override {}

 private:
  explicit ArduinoEsp32WiFiNetwork();
  void TaskFunctionInner();
  static void WiFiDhcpTimerCallback(TimerHandle_t timer);
  void WiFiDhcpTimerCallbackInner();
  static void TaskFunction(void* parameters);
  void reconnectToWiFi(Milliseconds currentTime, bool force = false);

  NetworkDeviceId localDeviceId_;  // TODO fix race condition.
  std::atomic<Milliseconds> lastReceiveTime_;
  bool attemptingDhcp_ = true;  // Can only be accessed from EspWiFi task.
  bool reconnecting_ = false;   // Can only be accessed from EspWiFi task.
  std::mutex mutex_;
  bool hasDataToSend_ = false;                  // Protected by mutex_.
  NetworkMessage messageToSend_;                // Protected by mutex_.
  std::list<NetworkMessage> receivedMessages_;  // Protected by mutex_.
};

}  // namespace jazzlights

#endif  // JL_ARDUINO_ESP32_WIFI
#endif  // ESP32
#endif  // JL_NETWORKS_ARDUINO_ESP32_WIFI_H
