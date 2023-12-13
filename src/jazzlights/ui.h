#ifndef JL_BUTTON_H
#define JL_BUTTON_H

#include "jazzlights/config.h"

#ifdef ARDUINO

#ifndef JL_BUTTON_LOCK
#define JL_BUTTON_LOCK (!JL_DEV)
#endif  // JL_BUTTON_LOCK

#include "jazzlights/network.h"
#include "jazzlights/player.h"

namespace jazzlights {

void arduinoUiInitialSetup(Player& player, Milliseconds currentTime);
void arduinoUiFinalSetup(Player& player, Milliseconds currentTime);
void arduinoUiLoop(Player& player, const Network& wifiNetwork, const Network& bleNetwork, Milliseconds currentMillis);
uint8_t getBrightness();

}  // namespace jazzlights

#endif  // ARDUINO

#endif  // JL_BUTTON_H