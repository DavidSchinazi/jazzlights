#ifndef JAZZLIGHTS_NETWORK_UDPSOCKET_H
#define JAZZLIGHTS_NETWORK_UDPSOCKET_H

#ifndef ARDUINO

#include "jazzlights/network.h"

#include <netinet/in.h>
#include <string>
#include <unordered_map>

namespace jazzlights {

class UnixUdpNetwork : public UdpNetwork {
 public:
  UnixUdpNetwork(int p = DEFAULT_UDP_PORT,
                 const char* addr = DEFAULT_MULTICAST_ADDR);
  UnixUdpNetwork(const UnixUdpNetwork&) = default;

  NetworkStatus update(NetworkStatus status, Milliseconds currentTime) override;
  NetworkDeviceId getLocalDeviceId() override {
    return localDeviceId_;
  }
  int recv(void* buf, size_t bufsize, std::string* details) override;
  void send(void* buf, size_t bufsize) override;
  const char* networkName() const override {
    return "UnixUDP";
  }
private:
  int setupSocketForInterface(const char* ifName, struct in_addr localAddr);
  void invalidateSocket(std::string ifName);
  bool setupSockets();

  NetworkDeviceId localDeviceId_;
  struct in_addr mcastAddr_;
  char mcastAddrStr_[sizeof("255.255.255.255")];
  const uint16_t port_;
  std::unordered_map<std::string, int> sockets_;
};

}  // namespace jazzlights

#endif  // ARDUINO

#endif  // JAZZLIGHTS_NETWORK_UDPSOCKET_H
