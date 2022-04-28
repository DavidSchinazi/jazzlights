#ifndef UNISPARKS_BUTTON_H
#define UNISPARKS_BUTTON_H

#include "unisparks/config.h"
#include "unisparks/player.hpp"

#if WEARABLE

namespace unisparks {

void setupButtons();
void doButtons(Player& player, NetworkStatus networkStatus, const Milliseconds currentMillis);
uint8_t getBrightness();

} // namespace unisparks

#endif // WEARABLE

#endif // UNISPARKS_BUTTON_H
