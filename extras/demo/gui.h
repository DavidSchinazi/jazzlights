#ifndef JL_EXTRAS_DEMO_GUI_H
#define JL_EXTRAS_DEMO_GUI_H

#include "jazzlights/player.h"

namespace jazzlights {

int runGui(const char* winTitle, Player& player, Box viewport, bool fullscreen, Milliseconds killTime);

}  // namespace jazzlights

#endif  // JL_EXTRAS_DEMO_GUI_H
