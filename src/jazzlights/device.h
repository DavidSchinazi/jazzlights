#ifndef JL_DEVICE_H
#define JL_DEVICE_H

#include "jazzlights/config.h"

#if RENDERABLE

#include <string>

#include "jazzlights/pattern_player.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

void deviceSetup(void);
void deviceLoop(void);

std::string wifiStatus(Milliseconds currentTime);
std::string bleStatus(Milliseconds currentTime);
std::string otherStatus(PatternPlayer& player, Milliseconds currentTime);

}  // namespace jazzlights

#endif  // RENDERABLE

#endif  // JL_DEVICE_H
