#ifndef UNISPARKS_FRAME_HPP
#define UNISPARKS_FRAME_HPP

#include <vector>

#include "unisparks/types.h"
#include "unisparks/util/geom.hpp"
#include "unisparks/util/time.hpp"
#include "unisparks/util/color.hpp"

namespace unisparks {

class PredictableRandom;

struct Frame {
 public:
  PatternBits pattern;
  PredictableRandom* predictableRandom = nullptr;
  Box viewport;
  void* context = nullptr;
  Milliseconds time;
  int pixelCount;
  std::vector<Coord> xValues;
  std::vector<Coord> yValues;
};

constexpr Coord width(const Frame& frame) {
  return frame.viewport.size.width;
}

constexpr Coord height(const Frame& frame) {
  return frame.viewport.size.height;
}

constexpr Point center(const Frame& frame) {
  return center(frame.viewport);
}

constexpr Point lefttop(const Frame& frame) {
  return frame.viewport.origin;
}

inline uint8_t* ucontext(const Frame& frame) {
  return reinterpret_cast<uint8_t*>(frame.context);
}

template<typename T>
inline T& cast_context(const Frame& frame) {
  return *reinterpret_cast<T*>(frame.context);
}

} // namespace unisparks
#endif /* UNISPARKS_FRAME_HPP */
