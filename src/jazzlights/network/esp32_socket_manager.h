#ifndef JL_NETWORK_ESP32_SOCKET_MANAGER_H
#define JL_NETWORK_ESP32_SOCKET_MANAGER_H

#include "jazzlights/util/config.h"

#if JL_WIFI || JL_ESP32_ETHERNET

#include <esp_event.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>

#include <atomic>
#include <mutex>
#include <vector>

#include "jazzlights/network/network.h"

namespace jazzlights {

// Manages one UDP multicast socket per registered network interface, all from a single task. The links themselves
// (e.g., Esp32WiFiLink) are managed separately. Each interface gets its own NetworkId so that messages received on one
// interface are relayed to the others but not echoed back onto the interface they came from.
class Esp32SocketManager : public Network {
 public:
  static Esp32SocketManager* Get();
  ~Esp32SocketManager();

  // Starts managing a socket on this interface. The socket is open whenever the interface has an IPv4 address.
  // The first interface's localDeviceId is used as ours. Must only be called from the primary runloop.
  void AddInterface(esp_netif_t* netif, NetworkType type, NetworkDeviceId localDeviceId);

  NetworkStatus Update(NetworkStatus /*status*/) override { return kConnected; }
  NetworkDeviceId GetLocalDeviceId() const override { return localDeviceId_; }
  NetworkType Type() const override { return NetworkType::kOther; }
  std::string GetStatusStr() override;
  void SetMessageToSend(const ProtocolMessage& messageToSend) override;
  void DisableSending() override;
  void TriggerSendAsap() override;
  // We perform echo suppression ourselves, per interface.
  bool ShouldEcho() const override { return true; }
  OptionalMicroseconds GetLastReceiveTime() const override { return lastReceiveTime_.load(std::memory_order_relaxed); }

 protected:
  std::list<ProtocolMessage> GetReceivedMessagesImpl() override;
  void RunLoopImpl() override {}

 private:
  struct Event {
    enum class Type {
      kReserved = 0,
      kInterfaceAdded,
      kGotIp,
      kLostIp,
    };
    Type type = Type::kReserved;
    esp_netif_t* netif = nullptr;
    NetworkType networkType = NetworkType::kOther;  // Only used for kInterfaceAdded.
    NetworkId networkId = 0;                        // Only used for kInterfaceAdded.
    struct in_addr address = {};                    // Only used for kGotIp.
  };
  struct Interface {
    esp_netif_t* netif;
    NetworkType type;
    NetworkId networkId;
    struct in_addr localAddress;
    int socket;
  };
  struct InterfaceStatus {
    NetworkType type;
    struct in_addr localAddress;
    OptionalMicroseconds lastReceiveTime;
  };

  explicit Esp32SocketManager();
  static void EventHandler(void* eventHandlerArg, esp_event_base_t eventBase, int32_t eventId, void* eventData);
  void HandleEvent(esp_event_base_t eventBase, int32_t eventId, void* eventData);
  void PostEvent(const Event& event);
  void HandleQueueEvent(const Event& event);
  static void TaskFunction(void* parameters);
  void RunTask();
  bool HasOpenSocket() const;
  void SetLocalAddress(size_t index, const struct in_addr& localAddress);
  void CreateSocket(size_t index);
  void CloseSocket(size_t index);
  void MaybeSend();
  void Receive(size_t index);
  void WaitForReceiveOrSendTime();

  QueueHandle_t eventQueue_;
  TaskHandle_t taskHandle_ = nullptr;     // Only modified in constructor.
  struct in_addr multicastAddress_ = {};  // Only modified in constructor.
  NetworkDeviceId localDeviceId_;         // Only used on the primary runloop.
  std::vector<Interface> interfaces_;     // Only used on our task.
  std::vector<struct pollfd> pollFds_;    // Only used on our task. Kept as a member to reuse its allocation.
  uint8_t* udpPayload_ = nullptr;         // Only used on our task. Used for both sending and receiving.
  OptionalMicroseconds lastSendTime_;     // Only used on our task.
  PatternBits lastSentPattern_ = 0;       // Only used on our task.
  std::atomic<OptionalMicroseconds> lastReceiveTime_;
  std::mutex mutex_;
  std::vector<InterfaceStatus> interfaceStatuses_;  // Protected by mutex_. Same indices as interfaces_.
  bool hasDataToSend_ = false;                      // Protected by mutex_.
  ProtocolMessage messageToSend_;                   // Protected by mutex_.
  std::list<ProtocolMessage> receivedMessages_;     // Protected by mutex_.
};

}  // namespace jazzlights

#endif  // JL_WIFI || JL_ESP32_ETHERNET
#endif  // JL_NETWORK_ESP32_SOCKET_MANAGER_H
