#ifndef DFSPARKS_UDP_H
#define DFSPARKS_UDP_H
#include <atomic>
#include <functional>
#include <thread>
#include <vector>

namespace dfsparks {

class Socket {
public:
  Socket();
  ~Socket();

  Socket &set_reuseaddr(bool v);
  Socket &set_broadcast(bool v);
  Socket &set_nonblocking(bool v);
  Socket &set_rcvtimeout(uint64_t millis);
  Socket &setopt(int name, const void *val, size_t valsize);

  void bind(const char *host, int port);
  void sendto(const char *host, int port, void *data, size_t len);
  size_t recvfrom(void *buf, size_t len, int flags = 0);

private:
  int fd;
};

} // namespace dfsparks

#endif /* DFSPARKS_UDP_H */