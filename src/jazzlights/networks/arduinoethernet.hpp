#ifndef JAZZLIGHTS_NETWORK_ARDUINO_ETHERNET_H
#define JAZZLIGHTS_NETWORK_ARDUINO_ETHERNET_H

#ifndef JAZZLIGHTS_ARDUINO_ETHERNET
#  define JAZZLIGHTS_ARDUINO_ETHERNET 0
#endif  // JAZZLIGHTS_ARDUINO_ETHERNET

#if JAZZLIGHTS_ARDUINO_ETHERNET

#include "jazzlights/network.hpp"
#include <SPI.h>
#include <Ethernet.h>

namespace jazzlights {

class ArduinoEthernetNetwork : public UdpNetwork {
 public:
  explicit ArduinoEthernetNetwork(NetworkDeviceId localDeviceId);

  NetworkStatus update(NetworkStatus status, Milliseconds currentTime) override;
  int recv(void* buf, size_t bufsize, std::string* details) override;
  void send(void* buf, size_t bufsize) override;
  NetworkDeviceId getLocalDeviceId() override {
    return localDeviceId_;
  }
  const char* networkName() const override {
    return "ArduinoEthernet";
  }
 private:
  NetworkDeviceId localDeviceId_;
  uint16_t port_ = DEFAULT_UDP_PORT;
  const char* mcastAddr_ = DEFAULT_MULTICAST_ADDR;
  EthernetUDP udp_;
};

}  // namespace jazzlights

#endif  // JAZZLIGHTS_ARDUINO_ETHERNET

#endif  // JAZZLIGHTS_NETWORK_ARDUINO_ETHERNET_H
