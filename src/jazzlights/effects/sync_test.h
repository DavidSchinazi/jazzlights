#ifndef JL_EFFECTS_SYNC_TEST_H
#define JL_EFFECTS_SYNC_TEST_H

#include "jazzlights/effects/functional.h"

namespace jazzlights {

inline FunctionalEffect sync_test() {
  return effect("synctest", [](const Frame& frame) {
    static const Color colors[] = {CRGB::Black, CRGB::Green, CRGB::Blue, CRGB::White};
    const size_t index = static_cast<size_t>(frame.time / 1000) % (sizeof(colors) / sizeof(colors[0]));
    const Color color = colors[index];
    return [color](const Pixel& /*pt*/) -> Color { return color; };
  });
};

}  // namespace jazzlights
#endif  // JL_EFFECTS_SYNC_TEST_H
