#ifndef JL_CORE2_H
#define JL_CORE2_H

#if RENDERABLE && CORE2AWS

#include "jazzlights/pattern_player.h"

namespace jazzlights {

void core2SetupStart(PatternPlayer& player, Milliseconds currentTime);
void core2SetupEnd(PatternPlayer& player, Milliseconds currentTime);
void core2Loop(PatternPlayer& player, Milliseconds currentTime);
uint8_t getBrightness();

}  // namespace jazzlights

#endif  // RENDERABLE && CORE2AWS
#endif  // JL_CORE2_H
