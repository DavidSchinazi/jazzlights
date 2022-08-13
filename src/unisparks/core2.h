#ifndef UNISPARKS_CORE2_H
#define UNISPARKS_CORE2_H

#if WEARABLE && CORE2AWS

#include "unisparks/player.hpp"

namespace unisparks {

void core2SetupStart(Player& player);
void core2SetupEnd(Player& player);
void core2Loop(Player& player, Milliseconds currentTime);

}  // namespace unisparks

#endif  // WEARABLE && CORE2AWS
#endif  // UNISPARKS_CORE2_H
