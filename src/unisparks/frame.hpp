#ifndef UNISPARKS_FRAME_HPP
#define UNISPARKS_FRAME_HPP
#include "unisparks/util/geom.hpp"
#include "unisparks/util/time.hpp"
#include "unisparks/util/rhytm.hpp"
#include "unisparks/util/color.hpp"

namespace unisparks {

struct Animation {
  // size_t pixelCount;
  Box viewport;
  void* context;
};

struct Frame {
  Animation animation;
  Milliseconds time;
  BeatsPerMinute tempo;
  Metre metre;
};

struct Pixel {
  Frame frame;
  size_t index;
  Point coord;
};


constexpr Coord width(const Frame& frame) {
  return frame.animation.viewport.size.width;
}

constexpr Coord height(const Frame& frame) {
  return frame.animation.viewport.size.height;
}

constexpr Point center(const Frame& frame) {
  return center(frame.animation.viewport);
}

constexpr Point lefttop(const Frame& frame) {
  return frame.animation.viewport.origin;
}

double pulse(const Frame& frame);

uint8_t cycleHue(const Frame& frame);

Milliseconds adjustDuration(const Frame& f, Milliseconds d);

} // namespace unisparks
#endif /* UNISPARKS_FRAME_HPP */
