#ifndef DFSPARKS_NETWORK_H
#define DFSPARKS_NETWORK_H
#include "DFSparks_Player.h"
#include "DFSparks_Timer.h"
#include "DFSparks_Log.h"
#include <stddef.h>
#include <stdint.h>

namespace dfsparks {

class NetworkPlayer : protected Player {
public:
  enum Mode { offline, client, server };

  void begin(Mode m = client);
  void end();

  using Player::play;
  using Player::next;
  using Player::prev;

  void render();

private:
  Mode mode;
  int32_t received_time = -1;
  int32_t sent_time = -1;
  int32_t sent_effect_id = -1;
  int32_t send_interval = 500;

  virtual int on_start_listening(int port) = 0;
  virtual int on_stop_listening() = 0;
  virtual int on_recv(char *buf, size_t bufsize) = 0;

  virtual int on_start_serving() = 0;
  virtual int on_stop_serving() = 0;
  virtual int on_broadcast(int port, char *buf, size_t bufsize) = 0;
};

#ifndef ARDUINO
class UdpSocketPlayer : public NetworkPlayer {
private:
  int on_start_listening(int port) override;
  int on_stop_listening() override;
  int on_recv(char *buf, size_t bufsize) override;
  int on_start_serving() override;
  int on_stop_serving() override;
  int on_broadcast(int port, char *buf, size_t bufsize) override;

  int fd;
};
#endif // ARDUINO

template <typename UDP> class WiFiUdpPlayer : public NetworkPlayer {
private:
  virtual int on_start_listening(int port) { 
    int res = udp.begin(port);
    if (res) {
      info("listening on port %d", port);
    }
    else {
      error("couldn't start listening on port %d", port);
    }
    return res; 
  }
  virtual int on_stop_listening() { udp.stop(); return 0; }
  virtual int on_recv(char *buf, size_t bufsize) {
    int cb = udp.parsePacket();
    if (cb <= 0) {
      return 0;
    }
    return udp.read(buf, bufsize);
  }

  virtual int on_start_serving() {
    // udp.begin();
    return 0;
  }
  virtual int on_stop_serving() {
    // udp.stop();
    return 0;
  }
  virtual int on_broadcast(int port, char *buf, size_t bufsize) {
    // IPAddress ip(255, 255, 255, 255);
    // udp.beginPacket(ip, port);
    // udp.write(buf, bufsize);
    // udp.endPacket();
    return 0;
  }

  UDP udp;
};

} // namespace dfsparks

#endif /* DFSPARKS_NETWORK_H */