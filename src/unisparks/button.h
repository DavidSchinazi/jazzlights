#ifndef UNISPARKS_BUTTON_H
#define UNISPARKS_BUTTON_H

#include "unisparks/config.h"
#include "unisparks/player.hpp"

namespace unisparks {

void setupButtons();
void doButtons(Player& player, uint32_t currentMillis);
void updateButtons(uint32_t currentMillis);
void pushBrightness(void);
uint8_t getBrightness();

} // namespace unisparks

#endif // UNISPARKS_BUTTON_H
