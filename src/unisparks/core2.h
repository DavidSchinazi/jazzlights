#ifndef UNISPARKS_CORE2_H
#define UNISPARKS_CORE2_H

#if WEARABLE && CORE2AWS

#include "unisparks/player.hpp"

namespace unisparks {

void core2SetupStart(Player& player, Milliseconds currentTime);
void core2SetupEnd(Player& player, Milliseconds currentTime);
void core2Loop(Player& player, Milliseconds currentTime);
uint8_t getBrightness();

}  // namespace unisparks

#endif  // WEARABLE && CORE2AWS
#endif  // UNISPARKS_CORE2_H
