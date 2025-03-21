#ifndef JL_EFFECT_COLOREDBURSTS_H
#define JL_EFFECT_COLOREDBURSTS_H

#include "jazzlights/effect/effect.h"
#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/palette.h"
#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/math.h"

namespace jazzlights {

// Colored Bursts orginally inspired from Sound Reactive WLED.
// https://github.com/scottrbailey/WLED-Utils/blob/main/effects_sr.md
// https://github.com/atuline/WLED/blob/master/wled00/FX.cpp

enum : size_t {
  kMaxColorsBurstLines = 16,
  kColorsBurstSmallerGridSize = 100,
};

struct ColoredBurstsState {
  bool dot;
  bool grad;
  uint8_t hue;
  uint8_t numLines;
  uint8_t speed;
  uint8_t fadeScale;
  int curX1;
  int curY1;
  bool curInit1;
  int curX2[kMaxColorsBurstLines];
  int curY2[kMaxColorsBurstLines];
  bool curInit2[kMaxColorsBurstLines];
};

class ColoredBursts : public EffectWithPaletteXYIndexAndState<ColoredBurstsState, CRGB> {
 public:
  std::string effectNamePrefix(PatternBits /*pattern*/) const override { return "bursts"; }

  ColorWithPalette innerColor(const Frame& f, ColoredBurstsState* /*state*/, const Pixel& /*px*/) const override {
    return ColorWithPalette::OverrideColor(ps(f));
  }

  void innerBegin(const Frame& f, ColoredBurstsState* state) const override {
    state->dot = false;
    state->grad = true;
    state->hue = 0;
    state->numLines = f.predictableRandom->GetRandomNumberBetween(5, 10);
    uint8_t fadeAmount = f.predictableRandom->GetRandomNumberBetween(20, 50);
    state->fadeScale = 255 - fadeAmount;
    state->curInit1 = false;
    for (uint8_t i = 0; i < state->numLines; i++) { state->curInit2[i] = false; }
    state->speed = f.predictableRandom->GetRandomNumberBetween(3, 10);
    // All pixels are default-initialized to black using CRGB's default constructor.
  }

  void innerRewind(const Frame& f, ColoredBurstsState* state) const override {
    state->hue++;
    // Slightly fade all pixels.
    for (size_t x = 0; x < w(f); x++) {
      for (size_t y = 0; y < h(f); y++) { ps(f, x, y).nscale8(state->fadeScale); }
    }

    int x1 = jlbeatsin(2 + state->speed, f.time, 0, (w(f) - 1));
    int y1 = jlbeatsin(5 + state->speed, f.time, 0, (h(f) - 1));
    int& curX1 = state->curX1;
    int& curY1 = state->curY1;
    if (!state->curInit1) {
      state->curInit1 = true;
      curX1 = x1;
      curY1 = y1;
    }
    const int xMark = 4;
    const int yMark = 4;
    int x1Min = std::min(curX1, x1);
    int x1Max = std::max(curX1, x1);
    if (x1Max - x1Min > xMark) {
      x1Min = x1;
      x1Max = x1;
    }
    int y1Min = std::min(curY1, y1);
    int y1Max = std::max(curY1, y1);
    if (y1Max - y1Min > yMark) {
      y1Min = y1;
      y1Max = y1;
    }

    for (uint8_t i = 0; i < state->numLines; i++) {
      int x2 = jlbeatsin(1 + state->speed, f.time, 0, (w(f) - 1), i * 24);
      int y2 = jlbeatsin(3 + state->speed, f.time, 0, (h(f) - 1), i * 48 + 64);
      CRGB color = colorFromPalette(f, i * 255 / state->numLines + state->hue);
      int& curX2 = state->curX2[i];
      int& curY2 = state->curY2[i];
      if (!state->curInit2[i]) {
        state->curInit2[i] = true;
        curX2 = x2;
        curY2 = y2;
      }
      int x2Min = std::min(curX2, x2);
      int x2Max = std::max(curX2, x2);
      if (x2Max - x2Min > xMark) {
        x2Min = x2;
        x2Max = x2;
      }
      int y2Min = std::min(curY2, y2);
      int y2Max = std::max(curY2, y2);
      if (y2Max - y2Min > yMark) {
        y2Min = y2;
        y2Max = y2;
      }
      for (int x1t = x1Min; x1t <= x1Max; x1t++) {
        for (int x2t = x2Min; x2t <= x2Max; x2t++) {
          for (int y1t = y1Min; y1t <= y1Max; y1t++) {
            for (int y2t = y2Min; y2t <= y2Max; y2t++) { drawLine(f, state, x1t, x2t, y1t, y2t, color); }
          }
        }
      }
      curX2 = x2;
      curY2 = y2;
    }
    curX1 = x1;
    curY1 = y1;
    // blur2d(leds, 4);
  }

 private:
  void drawLine(const Frame& f, ColoredBurstsState* state, int x1, int x2, int y1, int y2, CRGB color) const {
    int xsteps = std::abs(x1 - y1) + 1;
    int ysteps = std::abs(x2 - y2) + 1;
    bool steppingX = xsteps >= ysteps;
    int steps = steppingX ? xsteps : ysteps;

    for (int i = 1; i <= steps; i++) {
      int dx = x1 + (x2 - x1) * i / steps;
      int dy = y1 + (y2 - y1) * i / steps;
      ps(f, dx, dy) += color;
      if (state->grad) { ps(f, dx, dy) %= (i * 255 / steps); }
      if (steppingX) {
        if (dx < x1 && dx < x2) {
          ps(f, dx + 1, dy) += color;
          if (state->grad) { ps(f, dx + 1, dy) %= (i * 255 / steps); }
        }
        if (dx > x1 && dx > x2) {
          ps(f, dx - 1, dy) += color;
          if (state->grad) { ps(f, dx - 1, dy) %= (i * 255 / steps); }
        }
      } else {
        if (dy < y1 && dy < y2) {
          ps(f, dx, dy + 1) += color;
          if (state->grad) { ps(f, dx, dy + 1) %= (i * 255 / steps); }
        }
        if (dy > y1 && dy > y2) {
          ps(f, dx, dy - 1) += color;
          if (state->grad) { ps(f, dx, dy - 1) %= (i * 255 / steps); }
        }
      }
    }

    if (state->dot) {  // add white point at the ends of line
      ps(f, x1, y1) += CRGB::White;
      ps(f, x2, y2) += CRGB::White;
    }
  }
};

}  // namespace jazzlights

#endif  // JL_EFFECT_COLOREDBURSTS_H
