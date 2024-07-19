#ifndef JL_NETWORKS_WIFI_H
#define JL_NETWORKS_WIFI_H

#include "jazzlights/networks/arduino_esp32_wifi.h"
#include "jazzlights/networks/arduino_esp_wifi.h"
#include "jazzlights/networks/esp32_wifi.h"

namespace jazzlights {

#if JL_ESP32_WIFI
using WiFiNetwork = Esp32WiFiNetwork;
#else   // JL_ESP32_WIFI
using WiFiNetwork = ArduinoEspWiFiNetwork;
#endif  // JL_ESP32_WIFI

}  // namespace jazzlights

#endif  // JL_NETWORKS_WIFI_H
