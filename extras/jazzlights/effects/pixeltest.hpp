#ifndef JAZZLIGHTS_EFFECTS_PIXELTEST_H
#define JAZZLIGHTS_EFFECTS_PIXELTEST_H
#include "jazzlights/effects/functional.h"
#include "jazzlights/util/math.h"

namespace jazzlights {

auto pixeltest = [](Milliseconds each=100) {
  return effect([ = ](const Frame & frame) {
    int px = frame.time/each % frame.pixelCount;
    int i = 0;
    int *pi = &i;
    return [ = ](Point /*pt*/) -> Color {
      info("pi=%p, i=%d", pi, *pi);
      return *pi == px ? GREEN : BLACK;
    };
  });
};

}  // namespace jazzlights
#endif  // JAZZLIGHTS_EFFECTS_PIXELTEST_H
