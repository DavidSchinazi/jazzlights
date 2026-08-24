#ifndef JL_EFFECT_THE_MATRIX_H
#define JL_EFFECT_THE_MATRIX_H

#include "jazzlights/effect/effect.h"
#include "jazzlights/util/pseudorandom.h"

namespace jazzlights {

struct MatrixState {
  FrameTimeMs fallInterval;
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
  void InnerBegin(const Frame& f, MatrixState* state) const override {
    state->fallInterval = f.predictableRandom->GetRandomNumberBetween(20, 40);
    state->spawnRate = f.predictableRandom->GetRandomNumberBetween(192, 255);
    state->fadeRate = f.predictableRandom->GetRandomNumberBetween(10, 40);
    state->maxTicks = f.predictableRandom->GetRandomNumberBetween(1, 5);
    state->currentTicks = 0;
    // Progress the effect 2*h times to get pixels on all rows.
    for (size_t y = 0; y < 2 * H(f); y++) { ProgressEffect(f, state); }
  }
  void InnerRewind(const Frame& frame, MatrixState* state) const override {
    // Only act every maxTicks ticks.
    state->currentTicks++;
    if (state->currentTicks < state->maxTicks) { return; }
    state->currentTicks = 0;
    ProgressEffect(frame, state);
  }

  CRGB InnerColor(const Frame& f, MatrixState* /*state*/, const Pixel& /*px*/) const override {
    const uint8_t p = Ps(f, X(f), Y(f));
    if (p == kMatrixSpawn) {
      return CRGB(175, 255, 175);
    } else if (p == 0) {
      return CRGB::Black;
    } else {
      return CRGB(27, 130, 39).nscale8(p);
    }
  }
  std::string EffectName(PatternBits /*pattern*/) const override { return "the-matrix"; }

 private:
  void ProgressEffect(const Frame& f, MatrixState* state) const {
    for (size_t y = H(f) - 1;; y--) {
      for (size_t x = 0; x < W(f); x++) {
        if (Ps(f, x, y) == kMatrixSpawn) {
          Ps(f, x, y) = kMatrixTrail;  // Create trail pixel.
          if (y < H(f) - 1) {
            Ps(f, x, y + 1) = kMatrixSpawn;  // Move spawn down.
          }
        }
      }
      if (y == 0) { break; }
    }

    // Fade all trail pixels.
    for (size_t x = 0; x < W(f); x++) {
      for (size_t y = 0; y < H(f); y++) {
        if (Ps(f, x, y) != kMatrixSpawn) {
          if (Ps(f, x, y) > state->fadeRate) {
            Ps(f, x, y) -= state->fadeRate;
          } else {
            Ps(f, x, y) = 0;
          }
        }
      }
    }

    // Spawn new pixel.
    if (f.predictableRandom->GetRandomByte() < state->spawnRate) {
      size_t spawnX = f.predictableRandom->GetRandomNumberBetween(0, W(f) - 1);
      Ps(f, spawnX, 0) = kMatrixSpawn;
    }
  }
};

}  // namespace jazzlights

#endif  // JL_EFFECT_THE_MATRIX_H
