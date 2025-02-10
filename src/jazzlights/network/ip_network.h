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

#include "jazzlights/config.h"
#include "jazzlights/network/network.h"

#ifdef ESP32
#include <lwip/inet.h>
#else
#include <netinet/in.h>
#endif

namespace jazzlights {

class IpInterfaceManager : public Network {
 public:
  static IpInterfaceManager* get();

  NetworkStatus update(NetworkStatus /*status*/, Milliseconds /*currentTime*/) override { return CONNECTED; }
  NetworkDeviceId getLocalDeviceId() const override { return localDeviceId_; }
  NetworkType type() const override { return NetworkType::kOther; }
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
  explicit IpInterfaceManager();
  static void TaskFunction(void* parameters);
  void RunTask();

  NetworkDeviceId localDeviceId_;
  struct in_addr multicastAddress_ = {};  // Only modified in constructor.
  int socket_ = -1;                       // Only used on our task.
  uint8_t* udpPayload_ = nullptr;         // Only used on our task. Used for both sending and receiving.
  Milliseconds lastSendTime_ = -1;        // Only used on our task.
  PatternBits lastSentPattern_ = 0;       // Only used on our task.
  std::atomic<Milliseconds> lastReceiveTime_;
  std::mutex mutex_;
  struct in_addr localAddress_ = {};            // Protected by mutex_.
  bool hasDataToSend_ = false;                  // Protected by mutex_.
  NetworkMessage messageToSend_;                // Protected by mutex_.
  std::list<NetworkMessage> receivedMessages_;  // Protected by mutex_.
};

}  // namespace jazzlights

#endif  // JL_USE_IP_INTERFACE_MANAGER
#endif  // JL_NETWORK_IP_NETWORK_H
