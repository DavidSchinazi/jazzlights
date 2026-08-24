#ifndef JL_EXTRAS_DEMO_GUI_H
#define JL_EXTRAS_DEMO_GUI_H

#include "jazzlights/render/player.h"

namespace jazzlights {

int RunGui(const char* winTitle, Player& player, Box viewport, bool fullscreen, OptionalMicroseconds killTime);

}  // namespace jazzlights

#endif  // JL_EXTRAS_DEMO_GUI_H
