#ifndef JL_LAYOUT_DATA_CLOUDS_H
#define JL_LAYOUT_DATA_CLOUDS_H

#include "jazzlights/layout_data.h"

#if JL_IS_CONFIG(CLOUDS)
#ifdef ARDUINO

#include "jazzlights/layout.h"

namespace jazzlights {

Layout* GetCloudsLayout();

}  // namespace jazzlights

#endif  // ARDUINO
#endif  // JL_IS_CONFIG(CLOUDS)
#endif  // JL_LAYOUT_DATA_CLOUDS_H
