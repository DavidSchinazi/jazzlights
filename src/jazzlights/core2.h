#ifndef JL_CORE2_H
#define JL_CORE2_H

#ifdef ARDUINO
#if CORE2AWS

#include "jazzlights/player.h"

namespace jazzlights {

void core2SetupStart(Player& player, Milliseconds currentTime);
void core2SetupEnd(Player& player, Milliseconds currentTime);
void core2Loop(Player& player, Milliseconds currentTime);
uint8_t getBrightness();

}  // namespace jazzlights

#endif  // CORE2AWS
#endif  // ARDUINO
#endif  // JL_CORE2_H
