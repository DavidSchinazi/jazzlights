#ifndef UNISPARKS_EFFECT_THEMATRIX_H
#define UNISPARKS_EFFECT_THEMATRIX_H

#include "unisparks/effect.hpp"
#include "unisparks/pseudorandom.h"
#include "unisparks/util/color.hpp"

namespace unisparks {

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

// TODO make this pattern look good on robot or remove from rotation
class TheMatrix : public XYIndexStateEffect<MatrixState, uint8_t> {
  public:
  void innerBegin(const Frame& frame, MatrixState* state) const override {
    state->fallInterval = frame.predictableRandom->GetRandomNumberBetween(20, 40);
    state->spawnRate = frame.predictableRandom->GetRandomNumberBetween(192, 255);
    state->fadeRate = frame.predictableRandom->GetRandomNumberBetween(10, 40);
    state->maxTicks = frame.predictableRandom->GetRandomNumberBetween(1, 5);
    state->currentTicks = 0;
    memset(&ps(0, 0), 0, sizeof(uint8_t) * w() * h());
    // Progess the effect 2*h times to get pixels on all rows.
    for (size_t y = 0; y < 2 * h() ; y++) {
      progressEffect(frame, state);
    }
  }
  void innerRewind(const Frame& frame, MatrixState* state) const override {
    // Only act every maxTicks ticks.
    state->currentTicks++;
    if (state->currentTicks < state->maxTicks) {
      return;
    }
    state->currentTicks = 0;
    progressEffect(frame, state);
  }

  Color innerColor(const Frame& /*frame*/, MatrixState* /*state*/, const Pixel& /*px*/) const override {
    const uint8_t p = ps(x(), y());
    if (p == kMatrixSpawn) {
      return RgbColor(175 ,255 ,175);
    } else if (p == 0) {
      return BLACK;
    } else {
      return nscale8(RgbColor(27, 130, 39), p);
    }
  }
  std::string effectName(PatternBits /*pattern*/) const override { return "the-matrix"; }
 private:
  void progressEffect(const Frame& frame, MatrixState* state) const {
    for (size_t y = h() - 1;; y--) {
      for (size_t x = 0; x < w(); x++) {
        if (ps(x, y) == kMatrixSpawn) {
          ps(x, y) = kMatrixTrail;  // Create trail pixel.
          if (y < h() - 1) {
            ps(x, y + 1) = kMatrixSpawn; // Move spawn down.
          }
        }
      }
      if (y == 0) { break; }
    }

    // Fade all trail pixels.
    for (size_t x = 0; x < w(); x++) {
      for (size_t y = 0; y < h(); y++) {
        if (ps(x, y) != kMatrixSpawn) {
          if (ps(x,y) > state->fadeRate) {
            ps(x, y) -= state->fadeRate;
          } else {
            ps(x, y) = 0;
          }
        }
      }
    }

    // Check for empty screen to ensure code spawn.
    bool emptyScreen = true;
    for (size_t x = 0; x < w(); x++) {
      for (size_t y = 0; y < h(); y++) {
        if (ps(x, y)) {
          emptyScreen = false;
          break;
        }
      }
      if (!emptyScreen) { break; }
    }

    // Spawn new pixel.
    if (frame.predictableRandom->GetRandomByte() < state->spawnRate || emptyScreen) {
      size_t spawnX = frame.predictableRandom->GetRandomNumberBetween(0, w() - 1);
      ps(spawnX, 0) = kMatrixSpawn;
    }
  }
};

} // namespace unisparks

#endif  // UNISPARKS_EFFECT_THEMATRIX_H
