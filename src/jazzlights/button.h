#ifndef JAZZLIGHTS_BUTTON_H
#define JAZZLIGHTS_BUTTON_H

#include "jazzlights/config.h"
#include "jazzlights/networks/arduino_esp_wifi.h"
#include "jazzlights/networks/esp32_ble.h"
#include "jazzlights/player.h"

#if RENDERABLE

namespace jazzlights {

void setupButtons();
void doButtons(Player& player, const Network& wifiNetwork,
#if ESP32_BLE
               const Network& bleNetwork,
#endif  // ESP32_BLE
               Milliseconds currentMillis);

#if !CORE2AWS
uint8_t getBrightness();
#endif  // !CORE2AWS

}  // namespace jazzlights

#endif  // RENDERABLE

#endif  // JAZZLIGHTS_BUTTON_H
