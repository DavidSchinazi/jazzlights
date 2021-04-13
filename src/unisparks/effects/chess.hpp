#ifndef UNISPARKS_EFFECTS_CHESS_H
#define UNISPARKS_EFFECTS_CHESS_H
#include "unisparks/effects/functional.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

auto chess = [](Color fgcolor, Color bgcolor) {
  return effect([ = ](const Frame & frame) {
    int cellsz = max(2.0, max(width(frame), height(frame)) / 16);

    return [ = ](const Pixel& px) -> Color {
      return int(px.coord.x / cellsz) % 2 ^ int(px.coord.y / cellsz) % 2 ? fgcolor : bgcolor;
    };
  });
};


} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_CHESS_H */
