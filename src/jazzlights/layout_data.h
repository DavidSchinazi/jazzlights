#ifndef JL_LAYOUT_DATA_H
#define JL_LAYOUT_DATA_H

#include "jazzlights/config.h"

#ifdef ARDUINO

#include "jazzlights/layout.h"

namespace jazzlights {

const Layout* GetLayout();
const Layout* GetLayout2();

}  // namespace jazzlights

#endif  // ARDUINO

#endif  // JL_LAYOUT_DATA_H
