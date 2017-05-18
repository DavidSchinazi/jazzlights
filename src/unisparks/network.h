#ifndef UNISPARKS_NETWORK_H
#define UNISPARKS_NETWORK_H
#include "unisparks/effect.h"
#include "unisparks/playlist.h"
#include <stddef.h>
#include <stdint.h>

namespace unisparks {

class Network;

struct NetworkFrame {
  const char *pattern;
  int32_t timeElapsed;
  int32_t timeSinceBeat;
};

class NetworkListener {

  virtual void onStatusChange(Network &network) = 0;
  virtual void onReceived(Network &network, const NetworkFrame &frame) = 0;

  friend class Network;
  NetworkListener *next_;
};

class Network {
public:
  enum Status {
    disconnected,
    connecting,
    connected,
    disconnecting,
    connection_failed
  };

  bool poll();
  bool broadcast(const NetworkFrame &frame);

  void add(NetworkListener &l);
  void remove(NetworkListener &l);

  void disconnect();
  void reconnect();
  bool isConnected() const {return status_ == connected; }

  Status status() const { return status_; }
  int32_t lastTxTime() const {return lastTxTime_;}
  int32_t lastRxTime() const {return lastRxTime_;}

protected:
  void setStatus(Status s);

private:
  virtual void doConnection() = 0;
  virtual int doReceive(void *buf, size_t bufsize) = 0;
  virtual int doBroadcast(void *buf, size_t bufsize) = 0;

  Status status_ = connecting;
  NetworkListener *first_ = nullptr;
  int32_t lastTxTime_ = 0;
  int32_t lastRxTime_ = 0;
};


constexpr int udp_port = 0xDF0D;
constexpr int msg_frame = 0xDF0002;
extern const char* const multicast_addr;

} // namespace unisparks

#endif /* UNISPARKS_NETWORK_H */
