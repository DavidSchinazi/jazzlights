#ifndef JL_NETWORKS_ARDUINO_ETHERNET_H
#define JL_NETWORKS_ARDUINO_ETHERNET_H

#include "jazzlights/config.h"

#if JL_ETHERNET && !JL_ESP32_ETHERNET

#include <Ethernet.h>
#include <SPI.h>

#include "jazzlights/network.h"

namespace jazzlights {

class ArduinoEthernetNetwork : public UdpNetwork {
 public:
  static ArduinoEthernetNetwork* get();

  NetworkStatus update(NetworkStatus status, Milliseconds currentTime) override;
  int recv(void* buf, size_t bufsize, std::string* details) override;
  void send(void* buf, size_t bufsize) override;
  NetworkDeviceId getLocalDeviceId() override { return localDeviceId_; }
  const char* networkName() const override { return "ArduinoEthernet"; }
  const char* shortNetworkName() const override { return "Eth"; }
  std::string getStatusStr(Milliseconds currentTime) override;

 private:
  explicit ArduinoEthernetNetwork();
  NetworkDeviceId localDeviceId_;
  uint16_t port_ = DefaultUdpPort();
  const char* mcastAddr_ = DefaultMulticastAddress();
  EthernetUDP udp_;
};

}  // namespace jazzlights

#endif  // JL_ETHERNET && !JL_ESP32_ETHERNET

#endif  // JL_NETWORKS_ARDUINO_ETHERNET_H
