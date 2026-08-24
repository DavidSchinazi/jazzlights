#ifndef JL_NETWORK_ARDUINO_ETHERNET_H
#define JL_NETWORK_ARDUINO_ETHERNET_H

#include "jazzlights/util/config.h"

#if JL_ETHERNET && !JL_ESP32_ETHERNET

#include <Ethernet.h>
#include <SPI.h>

#include "jazzlights/network/network.h"

namespace jazzlights {

class ArduinoEthernetNetwork : public UdpNetwork {
 public:
  static ArduinoEthernetNetwork* Get();

  NetworkStatus Update(NetworkStatus status) override;
  int Recv(void* buf, size_t bufsize, std::string* details) override;
  void Send(void* buf, size_t bufsize) override;
  NetworkDeviceId GetLocalDeviceId() const override { return localDeviceId_; }
  NetworkType Type() const override { return NetworkType::kEthernet; }
  std::string GetStatusStr() override;

 private:
  explicit ArduinoEthernetNetwork();

  static NetworkDeviceId QueryLocalDeviceId();

  const NetworkDeviceId localDeviceId_ = QueryLocalDeviceId();
  uint16_t port_ = DefaultUdpPort();
  const char* mcastAddr_ = DefaultMulticastAddress();
  EthernetUDP udp_;
};

}  // namespace jazzlights

#endif  // JL_ETHERNET && !JL_ESP32_ETHERNET

#endif  // JL_NETWORK_ARDUINO_ETHERNET_H
