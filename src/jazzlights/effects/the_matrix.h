#ifndef JL_EFFECT_THEMATRIX_H
#define JL_EFFECT_THEMATRIX_H

#include "jazzlights/effect.h"
#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/color.h"

namespace jazzlights {

struct MatrixState {
  Milliseconds fallInterval;
  uint8_t spawnRate;
  uint8_t fadeRate;
  uint8_t maxTicks;
  uint8_t currentTicks;
};

enum : uint8_t {
  kMatrixSpawn = 255,
  kMatrixTrail = kMatrixSpawn - 10,
};

class TheMatrix : public XYIndexStateEffect<MatrixState, uint8_t> {
 public:
  void innerBegin(const Frame& f, MatrixState* state) const override {
    state->fallInterval = f.predictableRandom->GetRandomNumberBetween(20, 40);
    state->spawnRate = f.predictableRandom->GetRandomNumberBetween(192, 255);
    state->fadeRate = f.predictableRandom->GetRandomNumberBetween(10, 40);
    state->maxTicks = f.predictableRandom->GetRandomNumberBetween(1, 5);
    state->currentTicks = 0;
    // Progress the effect 2*h times to get pixels on all rows.
    for (size_t y = 0; y < 2 * h(f); y++) { progressEffect(f, state); }
  }
  void innerRewind(const Frame& frame, MatrixState* state) const override {
    // Only act every maxTicks ticks.
    state->currentTicks++;
    if (state->currentTicks < state->maxTicks) { return; }
    state->currentTicks = 0;
    progressEffect(frame, state);
  }

  Color innerColor(const Frame& f, MatrixState* /*state*/, const Pixel& /*px*/) const override {
    const uint8_t p = ps(f, x(f), y(f));
    if (p == kMatrixSpawn) {
      return Color(175, 255, 175);
    } else if (p == 0) {
      return Black();
    } else {
      return Color(27, 130, 39).nscale8(p);
    }
  }
  std::string effectName(PatternBits /*pattern*/) const override { return "the-matrix"; }

 private:
  void progressEffect(const Frame& f, MatrixState* state) const {
    for (size_t y = h(f) - 1;; y--) {
      for (size_t x = 0; x < w(f); x++) {
        if (ps(f, x, y) == kMatrixSpawn) {
          ps(f, x, y) = kMatrixTrail;  // Create trail pixel.
          if (y < h(f) - 1) {
            ps(f, x, y + 1) = kMatrixSpawn;  // Move spawn down.
          }
        }
      }
      if (y == 0) { break; }
    }

    // Fade all trail pixels.
    for (size_t x = 0; x < w(f); x++) {
      for (size_t y = 0; y < h(f); y++) {
        if (ps(f, x, y) != kMatrixSpawn) {
          if (ps(f, x, y) > state->fadeRate) {
            ps(f, x, y) -= state->fadeRate;
          } else {
            ps(f, x, y) = 0;
          }
        }
      }
    }

    // Spawn new pixel.
    if (f.predictableRandom->GetRandomByte() < state->spawnRate) {
      size_t spawnX = f.predictableRandom->GetRandomNumberBetween(0, w(f) - 1);
      ps(f, spawnX, 0) = kMatrixSpawn;
    }
  }
};

}  // namespace jazzlights

#endif  // JL_EFFECT_THEMATRIX_H
