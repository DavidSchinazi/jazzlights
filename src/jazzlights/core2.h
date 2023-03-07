#ifndef JAZZLIGHTS_CORE2_H
#define JAZZLIGHTS_CORE2_H

#if RENDERABLE && CORE2AWS

#include "jazzlights/player.h"

namespace jazzlights {

void core2SetupStart(Player& player, Milliseconds currentTime);
void core2SetupEnd(Player& player, Milliseconds currentTime);
void core2Loop(Player& player, Milliseconds currentTime);
uint8_t getBrightness();

}  // namespace jazzlights

#endif  // RENDERABLE && CORE2AWS
#endif  // JAZZLIGHTS_CORE2_H
