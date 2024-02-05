#ifndef JL_FRAME_H
#define JL_FRAME_H

#include <vector>

#include "jazzlights/pseudorandom.h"
#include "jazzlights/types.h"
#include "jazzlights/util/color.h"
#include "jazzlights/util/geom.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

class PredictableRandom;

struct Frame {
 public:
  PatternBits pattern;
  PredictableRandom* predictableRandom = nullptr;
  const XYIndexStore* xyIndexStore = nullptr;
  Box viewport;
  void* context = nullptr;
  Milliseconds time;
  size_t pixelCount;
};

constexpr Coord width(const Frame& frame) { return frame.viewport.size.width; }

constexpr Coord height(const Frame& frame) { return frame.viewport.size.height; }

constexpr Point center(const Frame& frame) { return center(frame.viewport); }

constexpr Point lefttop(const Frame& frame) { return frame.viewport.origin; }

constexpr Point righttop(const Frame& frame) {
  return {frame.viewport.origin.x + frame.viewport.size.width, frame.viewport.origin.y};
}

constexpr Point leftbottom(const Frame& frame) {
  return {
      frame.viewport.origin.x,
      frame.viewport.origin.y + frame.viewport.size.height,
  };
}

constexpr Point rightbottom(const Frame& frame) {
  return {
      frame.viewport.origin.x + frame.viewport.size.width,
      frame.viewport.origin.y + frame.viewport.size.height,
  };
}

inline Coord diagonal(const Frame& frame) { return distance(lefttop(frame), rightbottom(frame)); }

}  // namespace jazzlights
#endif  // JL_FRAME_H
