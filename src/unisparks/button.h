#ifndef UNISPARKS_BUTTON_H
#define UNISPARKS_BUTTON_H

#include "unisparks/config.h"
#include "unisparks/player.hpp"
#include "unisparks/networks/esp32_ble.hpp"
#include "unisparks/networks/esp8266wifi.hpp"

#if WEARABLE

namespace unisparks {

void setupButtons();
void doButtons(Player& player,
               const Esp8266WiFi& wifiNetwork,
#if ESP32_BLE
               const Esp32BleNetwork& bleNetwork,
#endif  // ESP32_BLE
               Milliseconds currentMillis);
uint8_t getBrightness();

} // namespace unisparks

#endif // WEARABLE

#endif // UNISPARKS_BUTTON_H
