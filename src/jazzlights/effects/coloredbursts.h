#ifndef JAZZLIGHTS_EFFECTS_COLOREDBURSTS_H
#define JAZZLIGHTS_EFFECTS_COLOREDBURSTS_H

#include "jazzlights/effect.h"
#include "jazzlights/palette.h"
#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/math.h"
#include "jazzlights/util/noise.h"

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

class ColoredBursts : public EffectWithPaletteXYIndexAndState<ColoredBurstsState, RgbColor> {
public:
  std::string effectNamePrefix(PatternBits /*pattern*/) const override { return "bursts"; }

  ColorWithPalette innerColor(const Frame& /*frame*/,
                              ColoredBurstsState* /*state*/,
                              const Pixel& /*px*/) const override {
    return ColorWithPalette::OverrideColor(ps());
  }

  void innerBegin(const Frame& frame, ColoredBurstsState* state) const override {
    state->dot = false;
    state->grad = true;
    state->hue = 0;
    state->numLines = frame.predictableRandom->GetRandomNumberBetween(5, 10);
    uint8_t fadeAmount = frame.predictableRandom->GetRandomNumberBetween(20, 50);
    state->fadeScale = 255 - fadeAmount;
    state->curInit1 = false;
    for (uint8_t i = 0; i < state->numLines; i++) {
      state->curInit2[i] = false;
    }
    state->speed = frame.predictableRandom->GetRandomNumberBetween(3, 10);
    // Start all pixels black.
    for (size_t x = 0; x < w(); x++) {
      for (size_t y = 0; y < h(); y++) {
        ps(x, y) = RgbColor(0, 0, 0);
      }
    }
  }

  void innerRewind(const Frame& frame, ColoredBurstsState* state) const override {
    using namespace internal;

    state->hue++;
    // Slightly fade all pixels.
    for (size_t x = 0; x < w(); x++) {
      for (size_t y = 0; y < h(); y++) {
        ps(x, y) = nscale8(ps(x, y), state->fadeScale);
      }
    }

    int x1 = beatsin(2 + state->speed, frame.time, 0, (w() - 1));
    int y1 = beatsin(5 + state->speed, frame.time, 0, (h() - 1));
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
      int x2 = beatsin(1 + state->speed, frame.time, 0, (w() - 1), i * 24);
      int y2 = beatsin(3 + state->speed, frame.time, 0, (h() - 1), i * 48 + 64);
      RgbColor color = colorFromPalette(i * 255 / state->numLines + state->hue);
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
            for (int y2t = y2Min; y2t <= y2Max; y2t++) {
              drawLine(state, x1t, x2t, y1t, y2t, color);
            }
          }
        }
      }
      curX2 = x2;
      curY2 = y2;
    }
    curX1 = x1;
    curY1 = y1;
    //blur2d(leds, 4);
  }
 private:
  void drawLine(ColoredBurstsState* state, int x1, int x2, int y1, int y2, RgbColor color) const {
    int xsteps = std::abs(x1 - y1) + 1;
    int ysteps = std::abs(x2 - y2) + 1;
    bool steppingX = xsteps >= ysteps;
    int steps = steppingX ? xsteps : ysteps;

    for (int i = 1; i <= steps; i++) {
      int dx = x1 + (x2-x1) * i / steps;
      int dy = y1 + (y2-y1) * i / steps;
      ps(dx, dy) += color;
      if (state->grad) {
        ps(dx, dy) %= (i * 255 / steps);
      }
      if (steppingX) {
        if (dx < x1 && dx < x2) {
          ps(dx + 1, dy) += color;
          if (state->grad) {
            ps(dx + 1, dy) %= (i * 255 / steps);
          }
        }
        if (dx > x1 && dx > x2) {
          ps(dx - 1, dy) += color;
          if (state->grad) {
            ps(dx - 1, dy) %= (i * 255 / steps);
          }
        }
      } else {
        if (dy < y1 && dy < y2) {
          ps(dx, dy + 1) += color;
          if (state->grad) {
            ps(dx, dy + 1) %= (i * 255 / steps);
          }
        }
        if (dy > y1 && dy > y2) {
          ps(dx, dy - 1) += color;
          if (state->grad) {
            ps(dx, dy - 1) %= (i * 255 / steps);
          }
        }
      }
    }

    if (state->dot) { //add white point at the ends of line
      ps(x1, y1) += CRGB::White;
      ps(x2, y2) += CRGB::White;
    }
  }
};

}  // namespace jazzlights

#endif  // JAZZLIGHTS_EFFECTS_COLOREDBURSTS_H
