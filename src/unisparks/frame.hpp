#ifndef UNISPARKS_FRAME_HPP
#define UNISPARKS_FRAME_HPP
#include "unisparks/util/geom.hpp"
#include "unisparks/util/time.hpp"
#include "unisparks/util/rhytm.hpp"
#include "unisparks/util/color.hpp"

namespace unisparks {

struct Frame {
  Milliseconds time;
  BeatsPerMinute tempo;
  Metre metre;
  int pixelCount;
  Point origin;
  Dimensions size;
  void* context;
  void* animationContext;
};

constexpr Point center(const Frame& frame) {
  return {
    frame.origin.x + frame.size.width / 2, 
    frame.origin.y + frame.size.height / 2};
}

double pulse(const Frame& frame);

uint8_t cycleHue(const Frame& frame);

Milliseconds adjustDuration(const Frame& f, Milliseconds d);

} // namespace unisparks
#endif /* UNISPARKS_FRAME_HPP */
