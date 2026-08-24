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
  static UnixUdpNetwork* Get();
  NetworkStatus Update(NetworkStatus /*status*/) override { return kConnected; }
  NetworkDeviceId GetLocalDeviceId() const override { return localDeviceId_; }
  int Recv(void* buf, size_t bufsize, std::string* details) override;
  void Send(void* buf, size_t bufsize) override;
  NetworkType Type() const override { return NetworkType::kOther; }
  std::string GetStatusStr() override { return "UnixUDP"; }

 private:
  explicit UnixUdpNetwork();
  int SetupSocketForInterface(const char* ifName, struct in_addr localAddr, int ifIndex);
  void InvalidateSocket(std::string ifName);
  bool SetupSockets();

  static NetworkDeviceId QueryLocalDeviceId();

  const NetworkDeviceId localDeviceId_ = QueryLocalDeviceId();
  struct in_addr mcastAddr_;
  std::unordered_map<std::string, int> sockets_;
};

}  // namespace jazzlights

#endif  // ESP32

#endif  // JL_NETWORK_UNIX_UDP_H
