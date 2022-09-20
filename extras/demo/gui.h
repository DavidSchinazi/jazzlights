#ifndef JAZZLIGHTS_GUI_HPP
#define JAZZLIGHTS_GUI_HPP

#include "jazzlights/player.h"

namespace jazzlights {

int runGui(const char* winTitle, Player& player, Box viewport, bool fullscreen = false);

}  // namespace jazzlights

#endif  // JAZZLIGHTS_GUI_HPP
