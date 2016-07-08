#ifndef DFSPARKS_NETWORK_H
#define DFSPARKS_NETWORK_H
#include "dfsparks/log.h"
#include "dfsparks/timer.h"
#include <stddef.h>
#include <stdint.h>

namespace dfsparks {

constexpr size_t max_message_size = 256;

struct PlayerState {
  const char *effect_name;
  int32_t start_time;
  int32_t beat_time;
};

class Network {
public:
  virtual ~Network() = default;

  void begin();
  void end();

  void broadcast(const PlayerState &state);
  int sync(PlayerState *state);

private:
  char buf[max_message_size];

  int32_t send_interval = 500;
  int32_t recv_timeout = 5000;

  int32_t received_time = -1;
  int32_t sent_time = -1;

  PlayerState received_state;

  virtual int on_begin(int port) = 0;
  virtual int on_end() = 0;
  virtual int on_recv(char *buf, size_t bufsize) = 0;
  virtual int on_broadcast(int port, char *buf, size_t bufsize) = 0;
};

#ifndef ARDUINO
// ==========================================================================
//  UdpSocketNetwork
// ==========================================================================
class UdpSocketNetwork : public Network {
private:
  int on_begin(int port) final;
  int on_end() final;
  int on_recv(char *buf, size_t bufsize) final;
  int on_broadcast(int port, char *buf, size_t bufsize) final;

  int fd = 0;
};
#endif // ARDUINO

template <typename UDP> class WiFiUdpNetwork : public Network {
public:
  static WiFiUdpNetwork<UDP> &get_instance() {
    static WiFiUdpNetwork<UDP> inst;
    return inst;
  }

private:
  int on_begin(int port) final {
    int res = udp.begin(port);
    if (res) {
      info("listening on port %d", port);
    } else {
      error("couldn't start listening on port %d", port);
    }
    return res;
  }
  int on_end() final {
    udp.stop();
    return 0;
  }

  int on_recv(char *buf, size_t bufsize) final {
    int cb = udp.parsePacket();
    if (cb <= 0) {
      return 0;
    }
    return udp.read(buf, bufsize);
  }

  int on_broadcast(int port, char *buf, size_t bufsize) final {
    (void)port;
    (void)buf;
    (void)bufsize;
    // IPAddress ip(255, 255, 255, 255);
    // udp.beginPacket(ip, port);
    // udp.write(buf, bufsize);
    // udp.endPacket();
    return 0;
  }

  UDP udp;
};

// ==========================================================================
//  DummyNetwork
// ==========================================================================
class DummyNetwork : public Network {
private:
  virtual int on_begin(int /*port*/) final { return 0; };
  virtual int on_end() final { return 0; }
  virtual int on_recv(char * /*buf*/, size_t /*bufsize*/) final { return 0; }
  virtual int on_broadcast(int /*port*/, char * /*buf*/,
                           size_t /*bufsize*/) final {
    return 0;
  };
};

} // namespace dfsparks

#endif /* DFSPARKS_NETWORK_H */
