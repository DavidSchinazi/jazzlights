#ifndef JL_NETWORKS_ETHERNET_H
#define JL_NETWORKS_ETHERNET_H

#include "jazzlights/networks/arduino_ethernet.h"
#include "jazzlights/networks/esp32_ethernet.h"

#if JL_ETHERNET

namespace jazzlights {

#if JL_ESP32_ETHERNET
using EthernetNetwork = Esp32EthernetNetwork;
#else   // JL_ESP32_ETHERNET
using EthernetNetwork = ArduinoEthernetNetwork;
#endif  // JL_ESP32_ETHERNET

}  // namespace jazzlights

#endif  // JL_ETHERNET

#endif  // JL_NETWORKS_ETHERNET_H
