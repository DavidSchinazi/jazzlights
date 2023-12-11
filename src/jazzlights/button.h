#ifndef JL_BUTTON_H
#define JL_BUTTON_H

#include "jazzlights/config.h"

#ifdef ARDUINO
#if JL_ATOM_MATRIX

#include "jazzlights/networks/arduino_esp_wifi.h"
#include "jazzlights/networks/esp32_ble.h"
#include "jazzlights/player.h"

namespace jazzlights {

void arduinoUiInitialSetup(Player& player, Milliseconds currentTime);
void arduinoUiFinalSetup(Player& player, Milliseconds currentTime);
void arduinoUiLoop(Player& player, const Network& wifiNetwork, const Network& bleNetwork, Milliseconds currentMillis);
uint8_t getBrightness();

}  // namespace jazzlights

#endif  // JL_ATOM_MATRIX
#endif  // ARDUINO

#endif  // JL_BUTTON_H
