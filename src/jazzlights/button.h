#ifndef JL_BUTTON_H
#define JL_BUTTON_H

#include "jazzlights/config.h"
#include "jazzlights/networks/arduino_esp_wifi.h"
#include "jazzlights/networks/esp32_ble.h"
#include "jazzlights/player.h"

#ifdef ARDUINO

namespace jazzlights {

void setupButtons();
void doButtons(Player& player, const Network& wifiNetwork, const Network& bleNetwork, Milliseconds currentMillis);

#if !CORE2AWS
uint8_t getBrightness();
#endif  // !CORE2AWS

}  // namespace jazzlights

#endif  // ARDUINO

#endif  // JL_BUTTON_H
