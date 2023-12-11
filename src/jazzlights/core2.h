#ifndef JL_CORE2_H
#define JL_CORE2_H

#ifdef ARDUINO
#if CORE2AWS

#include "jazzlights/player.h"

namespace jazzlights {

void arduinoUiInitialSetup(Player& player, Milliseconds currentTime);
void arduinoUiFinalSetup(Player& player, Milliseconds currentTime);
void arduinoUiLoop(Player& player, const Network& wifiNetwork, const Network& bleNetwork, Milliseconds currentTime);
uint8_t getBrightness();

}  // namespace jazzlights

#endif  // CORE2AWS
#endif  // ARDUINO
#endif  // JL_CORE2_H
