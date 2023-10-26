#ifndef JAZZLIGHTS_BOARD_H
#define JAZZLIGHTS_BOARD_H

#include "jazzlights/config.h"

#ifdef ARDUINO

#include "jazzlights/layout.h"

namespace jazzlights {

const Layout* GetLayout();
const Layout* GetLayout2();

}  // namespace jazzlights

#endif  // ARDUINO

#endif  // JAZZLIGHTS_BOARD_H
