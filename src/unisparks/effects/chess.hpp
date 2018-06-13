#ifndef UNISPARKS_EFFECTS_CHESS_H
#define UNISPARKS_EFFECTS_CHESS_H
#include "unisparks/effects/functional.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

constexpr auto chess = [](Color fgcolor, Color bgcolor) {
  return effect([ = ](const Frame & frame) {
    int width = frame.size.width;
    int height = frame.size.height;
    int cellsz = max(2, max(width, height) / 16);

    return [ = ](Point pt) -> Color {
      return int(pt.x / cellsz) % 2 ^ int(pt.y / cellsz) % 2 ? fgcolor : bgcolor;
    };
  });
};


} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_CHESS_H */
