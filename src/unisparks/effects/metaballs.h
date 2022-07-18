#ifndef UNISPARKS_EFFECTS_METABALLS_H
#define UNISPARKS_EFFECTS_METABALLS_H

#include "unisparks/effect.hpp"
#include "unisparks/palette.h"
#include "unisparks/pseudorandom.h"
#include "unisparks/util/math.hpp"
#include "unisparks/util/noise.h"

namespace unisparks {

// Metaballs orginally inspired from Sound Reactive WLED.
// https://github.com/atuline/WLED/blob/master/wled00/FX.cpp

struct MetaballsState {
  float speed;
  Coord diagonalLength;
  Coord ballRadius;
  uint8_t multX1;
  uint8_t multY1;
  uint16_t noiseX2a;
  uint16_t noiseX2b;
  uint16_t noiseY2a;
  uint16_t noiseY2b;
  uint16_t noiseX3a;
  uint16_t noiseX3b;
  uint16_t noiseY3a;
  uint16_t noiseY3b;
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
    state->multX1 = frame.predictableRandom->GetRandomNumberBetween(10, 30); // 23
    state->multY1 = frame.predictableRandom->GetRandomNumberBetween(10, 30); // 28
    state->noiseX2a = frame.predictableRandom->GetRandomNumberBetween(100, 60000); // 25355
    state->noiseX2b = frame.predictableRandom->GetRandomNumberBetween(100, 60000); // 685
    state->noiseY2a = frame.predictableRandom->GetRandomNumberBetween(100, 60000); // 355
    state->noiseY2b = frame.predictableRandom->GetRandomNumberBetween(100, 60000); // 11685
    state->noiseX3a = frame.predictableRandom->GetRandomNumberBetween(100, 60000); // 55355
    state->noiseX3b = frame.predictableRandom->GetRandomNumberBetween(100, 60000); // 6685
    state->noiseY3a = frame.predictableRandom->GetRandomNumberBetween(100, 60000); // 25355
    state->noiseY3b =  frame.predictableRandom->GetRandomNumberBetween(100, 60000); // 22685
  }

  void innerRewind(const Frame& frame, MetaballsState* state) const override {
    using namespace internal;
    const Coord ox = frame.viewport.origin.x;
    const Coord oy = frame.viewport.origin.y;
    const Coord w = frame.viewport.size.width / 256.0;
    const Coord h = frame.viewport.size.height / 256.0;

    // TODO make these pixels less jerky in their movement
    state->p1.x = ox + w * beatsin8(state->multX1 * state->speed, frame.time, 0, 255);
    state->p1.y = oy + h * beatsin8(state->multY1 * state->speed, frame.time, 0, 255);

    state->p2.x = ox + w * inoise8(frame.time * state->speed, state->noiseX2a, state->noiseX2b);
    state->p2.y = oy + h * inoise8(frame.time * state->speed, state->noiseY2a, state->noiseY2b);

    state->p3.x = ox + w * inoise8(frame.time * state->speed, state->noiseX3a, state->noiseX3b);
    state->p3.y = oy + h * inoise8(frame.time * state->speed, state->noiseY3a, state->noiseY3b);
  }
};

}  // namespace unisparks

#endif  // UNISPARKS_EFFECTS_METABALLS_H
