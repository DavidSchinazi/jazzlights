#ifndef JL_NETWORK_IP_NETWORK_H
#define JL_NETWORK_IP_NETWORK_H

// This portion is work in progress.
// TODO finish this.

#ifndef JL_USE_IP_INTERFACE_MANAGER
#define JL_USE_IP_INTERFACE_MANAGER 0
#endif  // JL_USE_IP_INTERFACE_MANAGER

#if JL_USE_IP_INTERFACE_MANAGER

#include <atomic>
#include <mutex>

#include "jazzlights/network/network.h"
#include "jazzlights/util/config.h"

#ifdef ESP32
#include <lwip/inet.h>
#else
#include <netinet/in.h>
#endif

namespace jazzlights {

class IpInterfaceManager : public Network {
 public:
  static IpInterfaceManager* Get();

  NetworkStatus Update(NetworkStatus /*status*/) override { return kConnected; }
  NetworkDeviceId GetLocalDeviceId() const override { return localDeviceId_; }
  NetworkType Type() const override { return NetworkType::kOther; }
  std::string GetStatusStr() override;
  void SetMessageToSend(const ProtocolMessage& messageToSend) override;
  void DisableSending() override;
  void TriggerSendAsap() override;
  bool ShouldEcho() const override { return false; }
  OptionalMicroseconds GetLastReceiveTime() const override { return lastReceiveTime_.load(std::memory_order_relaxed); }

 protected:
  std::list<ProtocolMessage> GetReceivedMessagesImpl() override;
  void RunLoopImpl() override {}

 private:
  explicit IpInterfaceManager();
  static void TaskFunction(void* parameters);
  void RunTask();

  NetworkDeviceId localDeviceId_;
  struct in_addr multicastAddress_ = {};  // Only modified in constructor.
  int socket_ = -1;                       // Only used on our task.
  uint8_t* udpPayload_ = nullptr;         // Only used on our task. Used for both sending and receiving.
  OptionalMicroseconds lastSendTime_;     // Only used on our task.
  PatternBits lastSentPattern_ = 0;       // Only used on our task.
  std::atomic<OptionalMicroseconds> lastReceiveTime_;
  std::mutex mutex_;
  struct in_addr localAddress_ = {};             // Protected by mutex_.
  bool hasDataToSend_ = false;                   // Protected by mutex_.
  ProtocolMessage messageToSend_;                // Protected by mutex_.
  std::list<ProtocolMessage> receivedMessages_;  // Protected by mutex_.
};

}  // namespace jazzlights

#endif  // JL_USE_IP_INTERFACE_MANAGER
#endif  // JL_NETWORK_IP_NETWORK_H
