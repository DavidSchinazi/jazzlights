#ifndef UNISPARKS_NETWORK_H
#define UNISPARKS_NETWORK_H
#include <string.h> // memcpy, size_t
#include "unisparks/util/time.hpp"
#include "unisparks/util/rhytm.hpp"

#include <list>
#include <string>

namespace unisparks {

class Network;

#define ALL_NETWORK_STATUSES \
  X(INITIALIZING) \
  X(DISCONNECTED) \
  X(CONNECTING) \
  X(CONNECTED) \
  X(DISCONNECTING) \
  X(CONNECTION_FAILED)

enum NetworkStatus {
#define X(s) s,
  ALL_NETWORK_STATUSES
#undef X
};

std::string NetworkStatusToString(NetworkStatus status);

using PatternBits = uint32_t;
using Precedence = uint16_t;

class NetworkDeviceId {
 public:
  explicit NetworkDeviceId() {
    memset(&data_[0], 0, kNetworkDeviceIdSize);
  }
  explicit NetworkDeviceId(const uint8_t* data) {
    memcpy(&data_[0], data, kNetworkDeviceIdSize);
  }
  NetworkDeviceId(const NetworkDeviceId& other) :
    NetworkDeviceId(&other.data_[0]) {}
  NetworkDeviceId& operator=(const NetworkDeviceId& other) {
    memcpy(&data_[0], &other.data_[0], kNetworkDeviceIdSize);
    return *this;
  }
  uint8_t operator()(uint8_t i) const {
    return data_[i];
  }
  int compare(const NetworkDeviceId& other) const {
    return memcmp(&data_[0], &other.data_[0], kNetworkDeviceIdSize);
  }
  void writeTo(uint8_t* data) const { memcpy(data, &data_[0], kNetworkDeviceIdSize); }
  bool operator==(const NetworkDeviceId& other) const { return compare(other) == 0; }
  bool operator!=(const NetworkDeviceId& other) const { return compare(other) != 0; }
  bool operator< (const NetworkDeviceId& other) const { return compare(other) <  0; }
  bool operator<=(const NetworkDeviceId& other) const { return compare(other) <= 0; }
  bool operator> (const NetworkDeviceId& other) const { return compare(other) >  0; }
  bool operator>=(const NetworkDeviceId& other) const { return compare(other) >= 0; }
 private:
  static constexpr size_t kNetworkDeviceIdSize = 6;
  uint8_t data_[kNetworkDeviceIdSize];
};

#define DEVICE_ID_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define DEVICE_ID_HEX(addr) addr(0), addr(1), addr(2), addr(3), addr(4), addr(5)

struct NetworkMessage {
  NetworkDeviceId originator = NetworkDeviceId();
  NetworkDeviceId sender = NetworkDeviceId();
  PatternBits currentPattern = 0;
  PatternBits nextPattern = 0;
  Milliseconds elapsedTime = 0;
  Precedence precedence = 0;
  Milliseconds receiptTime = 0;
};

std::string displayBitsAsBinary(PatternBits p);

class Network {
 public:
  virtual ~Network() = default;

  // Set message to send during next send opportunity.
  virtual void setMessageToSend(const NetworkMessage& messageToSend,
                                Milliseconds currentTime) = 0;

  // Gets list of received messages since last call.
  std::list<NetworkMessage> getReceivedMessages(Milliseconds currentTime);

  // Called once per Arduino loop.
  void runLoop(Milliseconds currentTime);

  // Get current network status.
  NetworkStatus status() const;

  // Request an immediate send.
  virtual void triggerSendAsap(Milliseconds currentTime) = 0;

 protected:
  Network() = default;
  // Perform any work necessary to switch to requested state.
  virtual NetworkStatus update(NetworkStatus status, Milliseconds currentTime) = 0;
  // Gets list of received messages since last call.
  virtual std::list<NetworkMessage> getReceivedMessagesImpl(Milliseconds currentTime) = 0;
  // Called once per Arduino loop.
  virtual void runLoopImpl(Milliseconds currentTime) = 0;
 private:
  void checkStatus(Milliseconds currentTime);
  void reconnect(Milliseconds currentTime);

  NetworkStatus status_ = INITIALIZING;

  Milliseconds lastConnectionAttempt_ = 0;
  Milliseconds minBackoffTimeout_ = 1000;
  Milliseconds maxBackoffTimeout_ = 16000;
  Milliseconds backoffTimeout_ = minBackoffTimeout_;
};

class UdpNetwork : public Network {
 public:
  UdpNetwork() = default;
  ~UdpNetwork() = default;
  UdpNetwork(const UdpNetwork&) = default;

  void setMessageToSend(const NetworkMessage& messageToSend,
                        Milliseconds currentTime) override;
  void triggerSendAsap(Milliseconds currentTime) override;

 protected:
  std::list<NetworkMessage> getReceivedMessagesImpl(Milliseconds currentTime) override;
  void runLoopImpl(Milliseconds currentTime) override;
  virtual int recv(void* buf, size_t bufsize) = 0;
  virtual void send(void* buf, size_t bufsize) = 0;
 private:
  bool maybeHandleNotConnected(Milliseconds currentTime);
  NetworkMessage messageToSend_;

  bool isEffectMaster_ = false;
  PatternBits lastSentPattern_ = 0;

  Milliseconds effectLastTxTime_ = 0;
  Milliseconds effectLastRxTime_ = 0;
};


static constexpr int DEFAULT_UDP_PORT = 0xDF0D;
static constexpr const char* const DEFAULT_MULTICAST_ADDR = "239.255.223.01";

} // namespace unisparks

#endif /* UNISPARKS_NETWORK_H */
