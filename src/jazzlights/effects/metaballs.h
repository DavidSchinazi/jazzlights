#ifndef JAZZLIGHTS_EFFECTS_METABALLS_H
#define JAZZLIGHTS_EFFECTS_METABALLS_H

#include "jazzlights/effect.hpp"
#include "jazzlights/palette.h"
#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/math.hpp"
#include "jazzlights/util/noise.h"

namespace jazzlights {

// Metaballs orginally inspired from Sound Reactive WLED.
// https://github.com/scottrbailey/WLED-Utils/blob/main/effects_sr.md
// https://github.com/atuline/WLED/blob/master/wled00/FX.cpp

struct MetaballsState {
  float speed;
  Coord diagonalLength;
  Coord ballRadius;
  uint8_t multX1;
  uint8_t multY1;
  uint8_t multX2;
  uint8_t multY2;
  uint8_t multX3;
  uint8_t multY3;
  Point p1;
  Point p2;
  Point p3;
};

class Metaballs : public EffectWithPaletteAndState<MetaballsState> {
public:
  std::string effectNamePrefix(PatternBits /*pattern*/) const override { return "metaballs"; }
  ColorWithPalette innerColor(const Frame& /*frame*/,
                              const Pixel& px,
                              MetaballsState* state) const override {
    const Point p = px.coord;
    const Coord d1 = distance(p, state->p1);
    const Coord d2 = distance(p, state->p2);
    const Coord d3 = distance(p, state->p3);
    if (d1 < state->ballRadius || d2 < state->ballRadius || d3 < state->ballRadius) {
      return ColorWithPalette::OverrideCRGB(CRGB::White);
    }

    const Coord dist = (d1 * 2 + d2 + d3) * 256 / state->diagonalLength;
    const uint8_t dist8 = static_cast<uint8_t>(dist);
    const uint8_t color = dist8 == 0 ? 255 : 1000 / dist8;

    if (color > 0 and color < 60) {
      return color * 9;
    } else {
      return 0;
    }
  }

  void innerBegin(const Frame& frame, MetaballsState* state) const override {
    state->speed = frame.predictableRandom->GetRandomDoubleBetween(0.1, 0.5);
    state->diagonalLength = diagonal(frame);
    state->ballRadius = state->diagonalLength / 50.0;
    state->multX1 = frame.predictableRandom->GetRandomNumberBetween(10, 30);
    state->multY1 = frame.predictableRandom->GetRandomNumberBetween(10, 30);
    state->multX2 = frame.predictableRandom->GetRandomNumberBetween(10, 30);
    state->multY2 = frame.predictableRandom->GetRandomNumberBetween(10, 30);
    state->multX3 = frame.predictableRandom->GetRandomNumberBetween(10, 30);
    state->multY3 = frame.predictableRandom->GetRandomNumberBetween(10, 30);
  }

  void innerRewind(const Frame& frame, MetaballsState* state) const override {
    using namespace internal;
    const Coord ox = frame.viewport.origin.x;
    const Coord oy = frame.viewport.origin.y;
    const Coord w = frame.viewport.size.width / 256.0;
    const Coord h = frame.viewport.size.height / 256.0;

    state->p1.x = ox + w * beatsin8(state->multX1 * state->speed, frame.time, 0, 255);
    state->p1.y = oy + h * beatsin8(state->multY1 * state->speed, frame.time, 0, 255);

    state->p2.x = ox + w * beatsin8(state->multX2 * state->speed, frame.time, 0, 255, 24);
    state->p2.y = oy + h * beatsin8(state->multY2 * state->speed, frame.time, 0, 255, 112);

    state->p3.x = ox + w * beatsin8(state->multX3 * state->speed, frame.time, 0, 255, 48);
    state->p3.y = oy + h * beatsin8(state->multY3 * state->speed, frame.time, 0, 255, 160);
  }
};

}  // namespace jazzlights

#endif  // JAZZLIGHTS_EFFECTS_METABALLS_H
