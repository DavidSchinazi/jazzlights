#ifndef UNISPARKS_NETWORK_ARDUINO_ETHERNET_H
#define UNISPARKS_NETWORK_ARDUINO_ETHERNET_H

#ifndef UNISPARKS_ARDUINO_ETHERNET
#  define UNISPARKS_ARDUINO_ETHERNET 0
#endif // UNISPARKS_ARDUINO_ETHERNET

#if UNISPARKS_ARDUINO_ETHERNET

#include "unisparks/network.hpp"
#include <SPI.h>
#include <Ethernet.h>

namespace unisparks {

class ArduinoEthernetNetwork : public UdpNetwork {
 public:
  explicit ArduinoEthernetNetwork(NetworkDeviceId localDeviceId);

  NetworkStatus update(NetworkStatus status, Milliseconds currentTime) override;
  int recv(void* buf, size_t bufsize, std::string* details) override;
  void send(void* buf, size_t bufsize) override;
  NetworkDeviceId getLocalDeviceId() override {
    return localDeviceId_;
  }
  const char* name() const override {
    return "ArduinoEthernet";
  }
 private:
  NetworkDeviceId localDeviceId_;
  uint16_t port_ = DEFAULT_UDP_PORT;
  const char* mcastAddr_ = DEFAULT_MULTICAST_ADDR;
  EthernetUDP udp_;
};

} // namespace unisparks

#endif // UNISPARKS_ARDUINO_ETHERNET

#endif // UNISPARKS_NETWORK_ARDUINO_ETHERNET_H
