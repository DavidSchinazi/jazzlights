#ifndef JL_NETWORK_WIFI_H
#define JL_NETWORK_WIFI_H

#include "jazzlights/network/arduino_esp_wifi.h"
#include "jazzlights/network/esp32_wifi.h"

#if JL_WIFI

namespace jazzlights {

#if JL_ESP32_WIFI
using WiFiNetwork = Esp32WiFiNetwork;
#else   // JL_ESP32_WIFI
using WiFiNetwork = ArduinoEspWiFiNetwork;
#endif  // JL_ESP32_WIFI

}  // namespace jazzlights

#endif  // JL_WIFI

#endif  // JL_NETWORK_WIFI_H
