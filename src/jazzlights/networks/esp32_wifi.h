#ifndef JL_NETWORKS_ESP32_WIFI_H
#define JL_NETWORKS_ESP32_WIFI_H

#ifdef ESP32

#include "jazzlights/network.h"

// Esp32WiFiNetwork is currently work-in-progress.
// TODO: finish this and use it to replace ArduinoEspWiFiNetwork.
#define JL_ESP32_WIFI 0

#ifndef JL_ESP32_WIFI
#define JL_ESP32_WIFI 1
#endif  // JL_ESP32_WIFI

#if JL_ESP32_WIFI

#include <esp_wifi.h>
#include <lwip/inet.h>

#include <atomic>
#include <mutex>

namespace jazzlights {

class Esp32WiFiNetwork : public Network {
 public:
  static Esp32WiFiNetwork* get();
  ~Esp32WiFiNetwork();

  NetworkStatus update(NetworkStatus status, Milliseconds currentTime) override;
  NetworkDeviceId getLocalDeviceId() override { return localDeviceId_; }
  const char* networkName() const override { return "Esp32WiFi"; }
  const char* shortNetworkName() const override { return "WiFi"; }
  std::string getStatusStr(Milliseconds currentTime) const override;
  void setMessageToSend(const NetworkMessage& messageToSend, Milliseconds currentTime) override;
  void disableSending(Milliseconds currentTime) override;
  void triggerSendAsap(Milliseconds currentTime) override;
  bool shouldEcho() const override { return false; }
  Milliseconds getLastReceiveTime() const override { return lastReceiveTime_.load(std::memory_order_relaxed); }

 protected:
  std::list<NetworkMessage> getReceivedMessagesImpl(Milliseconds currentTime) override;
  void runLoopImpl(Milliseconds /*currentTime*/) override {}

 private:
  explicit Esp32WiFiNetwork();
  static void EventHandler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
  void CreateSocket();
  void CloseSocket();

  std::atomic<Milliseconds> lastReceiveTime_;
  NetworkDeviceId localDeviceId_;
  int socket_ = -1;  // TODO figure out if this needs to be protected by mutex_.
  struct in_addr localAddress_ = {};
  std::mutex mutex_;
  bool hasDataToSend_ = false;                  // Protected by mutex_.
  NetworkMessage messageToSend_;                // Protected by mutex_.
  std::list<NetworkMessage> receivedMessages_;  // Protected by mutex_.
};

}  // namespace jazzlights

#endif  // JL_ESP32_WIFI
#endif  // ESP32
#endif  // JL_NETWORKS_ESP32_WIFI_H
