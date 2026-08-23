#ifndef JL_FRAME_H
#define JL_FRAME_H

#include <vector>

#include "jazzlights/render/xy_index.h"
#include "jazzlights/util/geom.h"
#include "jazzlights/util/time.h"
#include "jazzlights/util/types.h"

namespace jazzlights {

class PredictableRandom;

using FrameTimeMs = int32_t;

struct Frame {
 public:
  PatternBits pattern;
  PredictableRandom* predictableRandom = nullptr;
  const XYIndexStore* xyIndexStore = nullptr;
  Box viewport;
  void* context = nullptr;
  FrameTimeMs time;
  size_t pixelCount;
};

inline void SetFrameTime(Frame& frame, Microseconds currentTime, Microseconds patternStartTime) {
  if (currentTime <= patternStartTime) {
    frame.time = 0;
  } else {
    int64_t frameTimeMs = (currentTime - patternStartTime) / kMicrosecondsPerMillisecond;
    if (frameTimeMs >= static_cast<int64_t>(std::numeric_limits<FrameTimeMs>::max())) {
      frame.time = std::numeric_limits<FrameTimeMs>::max();
    } else {
      frame.time = static_cast<FrameTimeMs>(frameTimeMs);
    }
  }
}

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
