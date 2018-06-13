#ifndef ARDUINO
#ifndef UNISPARKS_NETWORK_UDPSOCKET_H
#define UNISPARKS_NETWORK_UDPSOCKET_H
#include "unisparks/network.hpp"

namespace unisparks {

struct Udp : public Network {
  Udp(int p = DEFAULT_UDP_PORT,
      const char* addr = DEFAULT_MULTICAST_ADDR) : mcastAddr(addr), port(p), fd(0) {
  }
  Udp(const Udp&) = default;

  NetworkStatus update(NetworkStatus st) override;
  int recv(void* buf, size_t bufsize) override;
  void send(void* buf, size_t bufsize) override;

  const char* mcastAddr;
  int port;
  int fd;
};

} // namespace unisparks

#endif /* UNISPARKS_NETWORK_UDPSOCKET_H */
#endif /* !ARDUINO */
