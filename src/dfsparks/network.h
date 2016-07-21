#ifndef DFSPARKS_NETWORK_H
#define DFSPARKS_NETWORK_H
#include "dfsparks/effect.h"
#include "dfsparks/playlist.h"
#include <stddef.h>
#include <stdint.h>

namespace dfsparks {

constexpr int udp_port = 0xDF0D;
constexpr int msg_frame = 0xDF0002;
extern const char *const multicast_addr;

struct Message {
  int32_t msgcode;
  char reserved[12];
  struct Frame {
    char pattern[16];
    int32_t elapsed_ms;
    int32_t beat_ms;
    uint8_t hue_med;
    uint8_t hue_dev;
  } __attribute__((__packed__)) frame;  
} __attribute__((__packed__));

class Network;

class NetworkListener {

  virtual void onStatusChange(Network &network) = 0;
  virtual void onReceived(Network &network, const Message::Frame &frame) = 0;

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
  bool broadcast(const Message::Frame &frame);

  void add(NetworkListener &l);
  void remove(NetworkListener &l);

  void disconnect();
  void reconnect();
  bool isConnected() const {return status_ == connected; }

  Status status() const { return status_; }

protected:
  void setStatus(Status s);

private:
  virtual void doConnection() = 0;
  virtual int doReceive(void *buf, size_t bufsize) = 0;
  virtual int doBroadcast(void *buf, size_t bufsize) = 0;

  Status status_ = connecting;
  NetworkListener *first_ = nullptr;
};

} // namespace dfsparks

#endif /* DFSPARKS_NETWORK_H */
