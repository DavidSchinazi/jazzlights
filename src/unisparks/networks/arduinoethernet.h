#ifdef ARDUINO
#ifndef UNISPARKS_NETWORK_ARDUINO_ETHERNET_H
#define UNISPARKS_NETWORK_ARDUINO_ETHERNET_H
#include "unisparks/network.h"
#include <SPI.h>        
#include <Ethernet.h>
#include <EthernetUdp.h>

namespace unisparks {

class ArduinoEthernetNetwork : public Network {
public:
  ArduinoEthernetNetwork(MacAddress mac);

private:
  void doConnection() final;
  int doReceive(void *buf, size_t bufsize) final;
  int doBroadcast(void *buf, size_t bufsize) final;

  MacAddress mac_;
  EthernetUDP udp_;
};

} // namespace unisparks

#endif /* UNISPARKS_NETWORK_ARDUINO_ETHERNET_H */
#endif /* ARDUINO */
