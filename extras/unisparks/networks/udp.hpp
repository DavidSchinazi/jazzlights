#ifndef ARDUINO
#ifndef UNISPARKS_NETWORK_UDPSOCKET_H
#define UNISPARKS_NETWORK_UDPSOCKET_H
#include "unisparks/network.hpp"

#include <netinet/in.h>

namespace unisparks {

struct Udp : public Network {
  Udp(int p = DEFAULT_UDP_PORT,
      const char* addr = DEFAULT_MULTICAST_ADDR);
  Udp(const Udp&) = default;

  NetworkStatus update(NetworkStatus st) override;
  int recv(void* buf, size_t bufsize) override;
  void send(void* buf, size_t bufsize) override;
private:
  bool setupSockets();

  struct in_addr mcastAddr_;
  char mcastAddrStr_[sizeof("255.255.255.255")];
  const uint16_t port_;
  int fd_;
};

} // namespace unisparks

#endif /* UNISPARKS_NETWORK_UDPSOCKET_H */
#endif /* !ARDUINO */
