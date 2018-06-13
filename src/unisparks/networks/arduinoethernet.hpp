#ifndef UNISPARKS_NETWORK_ARDUINO_ETHERNET_H
#define UNISPARKS_NETWORK_ARDUINO_ETHERNET_H
#include "unisparks/network.hpp"
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

namespace unisparks {

struct MacAddress {
  MacAddress(uint8_t* b) {
    memcpy(bytes, b, sizeof(bytes));
  }

  MacAddress(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5,
             uint8_t b6) :
    bytes{b1, b2, b3, b4, b5, b6} {}

  uint8_t bytes[6];
};

Network& arduinoEthernet(MacAddress mac);

} // namespace unisparks

#endif /* UNISPARKS_NETWORK_ARDUINO_ETHERNET_H */
