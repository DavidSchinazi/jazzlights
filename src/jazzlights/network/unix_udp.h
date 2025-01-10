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
  explicit UnixUdpNetwork();

  NetworkStatus update(NetworkStatus status, Milliseconds currentTime) override;
  NetworkDeviceId getLocalDeviceId() const override { return localDeviceId_; }
  int recv(void* buf, size_t bufsize, std::string* details) override;
  void send(void* buf, size_t bufsize) override;
  NetworkType type() const override { return NetworkType::kOther; }
  std::string getStatusStr(Milliseconds /*currentTime*/) override { return "UnixUDP"; }

 private:
  int setupSocketForInterface(const char* ifName, struct in_addr localAddr);
  void invalidateSocket(std::string ifName);
  bool setupSockets();

  static NetworkDeviceId QueryLocalDeviceId();

  const NetworkDeviceId localDeviceId_ = QueryLocalDeviceId();
  struct in_addr mcastAddr_;
  std::unordered_map<std::string, int> sockets_;
};

}  // namespace jazzlights

#endif  // ESP32

#endif  // JL_NETWORK_UNIX_UDP_H
