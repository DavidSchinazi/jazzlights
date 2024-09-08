#ifndef JL_NETWORKS_WIFI_H
#define JL_NETWORKS_WIFI_H

#include "jazzlights/networks/arduino_esp_wifi.h"
#include "jazzlights/networks/esp32_wifi.h"

namespace jazzlights {

// Esp32WiFiNetwork is currently work-in-progress.
// TODO: finish this and use it to replace ArduinoEspWiFiNetwork.
#define JL_ESP32_WIFI 0

#if JL_ESP32_WIFI
using WiFiNetwork = Esp32WiFiNetwork;
#else   // JL_ESP32_WIFI
using WiFiNetwork = ArduinoEspWiFiNetwork;
#endif  // JL_ESP32_WIFI

}  // namespace jazzlights

#endif  // JL_NETWORKS_WIFI_H
