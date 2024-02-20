#ifndef JL_UTIL_COLOR_H
#define JL_UTIL_COLOR_H

#include <stdint.h>
#include <string.h>

#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/util/math.h"

namespace jazzlights {

using Color = CRGB;

static inline Color Black() { return Color(0x0); }
static inline Color Red() { return Color(0xff0000); }
static inline Color Green() { return Color(0x00ff00); }
static inline Color Blue() { return Color(0x0000ff); }
static inline Color Purple() { return Color(0x9C27B0); }
static inline Color Cyan() { return Color(0x00BCD4); }
static inline Color Yellow() { return Color(0xFFFF00); }
static inline Color White() { return Color(0xffffff); }

}  // namespace jazzlights

#endif  // JL_UTIL_COLOR_H
