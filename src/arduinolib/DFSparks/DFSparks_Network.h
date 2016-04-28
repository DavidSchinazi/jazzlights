#ifndef DFSPARKS_NETWORK_H
#define DFSPARKS_NETWORK_H
#include <stddef.h>
#include <stdint.h>
#include "DFSParks_Timer.h"

namespace dfsparks {

class Client {
public:
  static const int timeout = 5000;

  void begin();
  void update();

  int get_effect_id() const { return effect_id; }
  int32_t get_start_time() const { return effect_start_time; }

private:
  virtual int on_begin(int port) = 0;
  virtual int on_recv(char *buf, size_t bufsize) = 0;

  bool running = false;
  int effect_id = -1;
  int32_t effect_start_time = -1;
  int32_t received_time = -1;
};

class Server {
public:
  static const int interval = 1000;

  void begin();
  void update(int effect_id, int32_t elapsed_time);

private:
  virtual int on_begin() = 0;
  virtual int on_send(const char *addr, int port, char *buf,
                      size_t bufsize) = 0;

  bool running = false;
  int sent_effect_id = -1;
  int32_t sent_time = -1;
};

template <typename UDP> class WiFiUdpClient : public Client {
private:
  virtual int on_begin(int port) override { udp.begin(port); }

  virtual int on_recv(char *buf, size_t bufsize) override {
    int cb = udp.parsePacket();
    if (cb <= 0) {
      return 0;
    }
    return udp.read(buf, bufsize);
  }

  UDP udp;
};

} // namespace dfsparks

#endif /* DFSPARKS_NETWORK_H */