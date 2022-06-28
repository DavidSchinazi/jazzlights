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
  // TODO add effect pattern bits to frame and use it from effects.
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

inline Dimensions8 size8(const Frame& frame) {
  auto b = scaleToFit(frame.animation.viewport.size, {255,255});
  return {(uint8_t)b.width, (uint8_t)b.height};
}

inline Vec8 pos8(const Pixel& px) {
  auto v = translateInto(px.coord, px.frame.animation.viewport, {{255,255},{0,0}});
  return {(uint8_t)v.x, (uint8_t)v.y};
}

inline uint8_t* ucontext(const Frame& frame) {
  return reinterpret_cast<uint8_t*>(frame.animation.context);
}

template<typename T>
inline T& cast_context(const Frame& frame) {
  return *reinterpret_cast<T*>(frame.animation.context);
}

double pulse(const Frame& frame);

uint8_t cycleHue(const Frame& frame);
uint8_t cycleHue(const Frame& frame, Milliseconds periodHint);

Milliseconds adjustDuration(const Frame& f, Milliseconds d);

} // namespace unisparks
#endif /* UNISPARKS_FRAME_HPP */
