#ifndef JL_LAYOUT_DATA_CLOUDS_H
#define JL_LAYOUT_DATA_CLOUDS_H

#include "jazzlights/layout/layout_data.h"

#if JL_IS_CONFIG(CLOUDS)
#ifdef ESP32

#include "jazzlights/layout/layout.h"

namespace jazzlights {

Layout* GetCloudsLayout();

}  // namespace jazzlights

#endif  // ESP32
#endif  // JL_IS_CONFIG(CLOUDS)
#endif  // JL_LAYOUT_DATA_CLOUDS_H
