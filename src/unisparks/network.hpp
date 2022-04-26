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
using NetworkDeviceId = uint8_t[6];

#define DEVICE_ID_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define DEVICE_ID_HEX(addr) addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]

struct NetworkMessage {
  NetworkDeviceId originator;
  NetworkDeviceId sender;
  PatternBits currentPattern;
  PatternBits nextPattern;
  Milliseconds elapsedTime;
  Precedence precedence;
};

std::string displayBitsAsBinary(PatternBits p);

class Network {
 public:
  virtual ~Network() = default;

  virtual void setMessageToSend(const NetworkMessage& messageToSend,
                                Milliseconds currentTime) = 0;
  std::list<NetworkMessage> getReceivedMessages(Milliseconds currentTime);
  void runLoop(Milliseconds currentTime);

  NetworkStatus status() const;
  virtual void triggerSendAsap(Milliseconds currentTime) = 0;
  virtual void setup() = 0;

 protected:
  virtual NetworkStatus update(NetworkStatus s) = 0;
  virtual std::list<NetworkMessage> getReceivedMessagesImpl(Milliseconds currentTime) = 0;
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
  void setup() override {}

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
