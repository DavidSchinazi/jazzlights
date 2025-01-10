#ifndef JL_NETWORK_ESP32_WIFI_H
#define JL_NETWORK_ESP32_WIFI_H

#include "jazzlights/config.h"

#if JL_WIFI

#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <lwip/inet.h>

#include <atomic>
#include <mutex>

#include "jazzlights/network/network.h"

namespace jazzlights {

class Esp32WiFiNetwork : public Network {
 public:
  static Esp32WiFiNetwork* get();
  ~Esp32WiFiNetwork();

  NetworkStatus update(NetworkStatus status, Milliseconds currentTime) override;
  NetworkDeviceId getLocalDeviceId() const override { return localDeviceId_; }
  NetworkType type() const override { return NetworkType::kWiFi; }
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
  struct Esp32WiFiNetworkEvent {
    enum class Type {
      kReserved = 0,
      kStationStarted,
      kStationDisconnected,
      kGotIp,
      kLostIp,
      kSocketReady,
    };
    Type type;
    union {
      struct in_addr address;
    } data;
    explicit Esp32WiFiNetworkEvent(Type t) {
      memset(this, 0, sizeof(*this));
      type = t;
    }
    explicit Esp32WiFiNetworkEvent() : Esp32WiFiNetworkEvent(Type::kReserved) {}
  };
  explicit Esp32WiFiNetwork();
  static void EventHandler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
  void HandleEvent(esp_event_base_t event_base, int32_t event_id, void* event_data);
  void HandleNetworkEvent(const Esp32WiFiNetworkEvent& networkEvent);
  static void TaskFunction(void* parameters);
  void RunTask();
  void CreateSocket();
  void CloseSocket();

  NetworkDeviceId InitWiFiStackAndQueryLocalDeviceId();

  QueueHandle_t eventQueue_;
  const NetworkDeviceId localDeviceId_ = InitWiFiStackAndQueryLocalDeviceId();
  TaskHandle_t taskHandle_ = nullptr;               // Only modified in constructor.
  struct in_addr multicastAddress_ = {};            // Only modified in constructor.
  int socket_ = -1;                                 // Only used on our task.
  uint8_t* udpPayload_ = nullptr;                   // Only used on our task. Used for both sending and receiving.
  Milliseconds lastSendTime_ = -1;                  // Only used on our task.
  PatternBits lastSentPattern_ = 0;                 // Only used on our task.
  bool shouldArmQueueReconnectionTimeout_ = false;  // Only used on our task.
  uint32_t reconnectCount_ = 0;                     // Only used on our task.
  std::atomic<Milliseconds> lastReceiveTime_;
  std::mutex mutex_;
  struct in_addr localAddress_ = {};            // Protected by mutex_.
  bool hasDataToSend_ = false;                  // Protected by mutex_.
  NetworkMessage messageToSend_;                // Protected by mutex_.
  std::list<NetworkMessage> receivedMessages_;  // Protected by mutex_.
};

}  // namespace jazzlights

#endif  // JL_WIFI
#endif  // JL_NETWORK_ESP32_WIFI_H
