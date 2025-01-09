#ifndef JL_NETWORK_UNIX_UDP_H
#define JL_NETWORK_UNIX_UDP_H

#ifndef ESP32

#include <netinet/in.h>

#include <string>
#include <unordered_map>

#include "jazzlights/network/network.h"

namespace jazzlights {

class UnixUdpNetwork : public UdpNetwork {
 public:
  UnixUdpNetwork(uint16_t port = DefaultUdpPort(), const char* addr = DefaultMulticastAddress());

  NetworkStatus update(NetworkStatus status, Milliseconds currentTime) override;
  NetworkDeviceId getLocalDeviceId() override { return localDeviceId_; }
  int recv(void* buf, size_t bufsize, std::string* details) override;
  void send(void* buf, size_t bufsize) override;
  NetworkType type() const override { return NetworkType::kOther; }
  std::string getStatusStr(Milliseconds /*currentTime*/) override { return "UnixUDP"; }

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

#endif  // ESP32

#endif  // JL_NETWORK_UNIX_UDP_H
