#ifndef JL_VEST_H
#define JL_VEST_H

#include "jazzlights/config.h"

#if RENDERABLE

#include <string>

#include "jazzlights/player.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

void deviceSetup(void);
void deviceLoop(void);

std::string wifiStatus(Milliseconds currentTime);
std::string bleStatus(Milliseconds currentTime);
std::string otherStatus(Player& player, Milliseconds currentTime);

}  // namespace jazzlights

#endif  // RENDERABLE

#endif  // JL_VEST_H
