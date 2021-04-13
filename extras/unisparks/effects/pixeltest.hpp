#ifndef UNISPARKS_EFFECTS_PIXELTEST_H
#define UNISPARKS_EFFECTS_PIXELTEST_H
#include "unisparks/effects/functional.h"
#include "unisparks/util/math.h"

namespace unisparks {

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

} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_PIXELTEST_H */
