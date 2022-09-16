#ifndef JAZZLIGHTS_BUTTON_H
#define JAZZLIGHTS_BUTTON_H

#include "jazzlights/config.h"
#include "jazzlights/player.h"
#include "jazzlights/networks/esp32_ble.h"
#include "jazzlights/networks/esp8266wifi.h"

#if WEARABLE

namespace jazzlights {

void setupButtons();
void doButtons(Player& player,
               const Esp8266WiFi& wifiNetwork,
#if ESP32_BLE
               const Esp32BleNetwork& bleNetwork,
#endif  // ESP32_BLE
               Milliseconds currentMillis);

#if !CORE2AWS
uint8_t getBrightness();
#endif  // !CORE2AWS

}  // namespace jazzlights

#endif // WEARABLE

#endif  // JAZZLIGHTS_BUTTON_H
