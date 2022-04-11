#ifndef UNISPARKS_NETWORK_H
#define UNISPARKS_NETWORK_H
#include <string.h> // memcpy, size_t
#include "unisparks/util/time.hpp"
#include "unisparks/util/rhytm.hpp"

#include <string>

namespace unisparks {

using PatternBits = uint32_t;

std::string displayBitsAsBinary(PatternBits p);

class Network;

enum NetworkStatus {
  INITIALIZING,
  DISCONNECTED,
  CONNECTING,
  CONNECTED,
  DISCONNECTING,
  CONNECTION_FAILED
};

class Network {
 public:
  virtual ~Network() = default;

  void disconnect();
  void reconnect();

  bool sync(PatternBits* pattern, Milliseconds* time);

  NetworkStatus status() const;

  bool connected() const {
    return status_ == CONNECTED;
  }
  bool isControllingEffects() const;
  Network& isControllingEffects(bool v);

 private:
  virtual NetworkStatus update(NetworkStatus s) = 0;
  virtual int recv(void* buf, size_t bufsize) = 0;
  virtual void send(void* buf, size_t bufsize) = 0;

  NetworkStatus status_ = INITIALIZING;

  Milliseconds lastConnectionAttempt_ = 0;
  Milliseconds minBackoffTimeout_ = 1000;
  Milliseconds maxBackoffTimeout_ = 16000;
  Milliseconds backoffTimeout_ = minBackoffTimeout_;

  bool isEffectMaster_ = false;
  PatternBits pattern_ = 0;
  PatternBits lastSentPattern_ = 0;

  Milliseconds effectLastTxTime_ = 0;
  Milliseconds effectLastRxTime_ = 0;

  // bool isBeatMaster_ = false;
  // Milliseconds beatLastTxTime_ = 0;
  // Milliseconds beatLastRxTime_ = 0;
};


static constexpr int DEFAULT_UDP_PORT = 0xDF0D;
static constexpr const char* const DEFAULT_MULTICAST_ADDR = "239.255.223.01";

Network& dummyNetwork();

} // namespace unisparks

#endif /* UNISPARKS_NETWORK_H */
