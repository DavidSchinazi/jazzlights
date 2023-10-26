#ifndef JAZZLIGHTS_GEOM_H
#define JAZZLIGHTS_GEOM_H
#include <cmath>

#include "jazzlights/util/math.h"

namespace jazzlights {

// I originally tried integer math, but it seems like it doesn't make
// much difference neither on PC nor on arduino.
//
// Most likely this is because the main performance hog by far is conversion
// between RGB & HSL spaces, which I do in floating-point anyway. And I
// do it this way because everything I tried with fixed point so far looked
// like crap. I may try again if I figure out fast AND decent looking HSL/RGBA
// transform...
using Coord = double;
using Meters = double;

struct Vector2D {
  Coord x;
  Coord y;
};

struct Vector3D {
  Coord x;
  Coord y;
  Coord z;
};

using Point2D = Vector2D;
using Point3D = Vector3D;
using Point = Point2D;
using Origin = Point2D;

constexpr Coord EmptyCoord() { return std::numeric_limits<Coord>::quiet_NaN(); }
constexpr Point EmptyPoint() { return {EmptyCoord(), EmptyCoord()}; }
constexpr bool IsEmpty(Coord c) { return std::isnan(c); }
constexpr bool IsEmpty(Point p) { return IsEmpty(p.x) && IsEmpty(p.y); }

struct Dimensions {
  Coord width;
  Coord height;
};

struct Box {
  Dimensions size;
  Point origin;
};

struct Vec8 {
  uint8_t x;
  uint8_t y;
};

struct Dimensions8 {
  uint8_t width;
  uint8_t height;
};

constexpr Coord left(const Box& b) { return b.origin.x; }

constexpr Coord top(const Box& b) { return b.origin.y; }

constexpr Coord right(const Box& b) { return b.origin.x + b.size.width; }

constexpr Coord bottom(const Box& b) { return b.origin.y + b.size.height; }

constexpr Coord width(const Box& b) { return b.size.width; }

constexpr Coord height(const Box& b) { return b.size.height; }

constexpr Point center(Box b) { return {b.origin.x + b.size.width / 2, b.origin.y + b.size.height / 2}; }

inline Point operator+(const Point& a, const Point& b) { return {a.x + b.x, a.y + b.y}; }

inline Point operator*(double k, const Point& v) { return {Coord(k * v.x), Coord(k * v.y)}; }

inline double distance(Point a, Point b) { return sqrt(square(a.x - b.x) + square(a.y - b.y)); }

inline Box merge(const Box& a, const Box& b) {
  Coord left = std::min(a.origin.x, b.origin.x);
  Coord top = std::min(a.origin.y, b.origin.y);
  Coord right = std::max(a.origin.x + a.size.width, b.origin.x + b.size.width);
  Coord bottom = std::max(a.origin.y + a.size.height, b.origin.y + b.size.height);
  return {
      {right - left, bottom - top},
      {        left,          top}
  };
}

inline Box merge(const Box& a, const Point& p) {
  Coord left = std::min(a.origin.x, p.x);
  Coord top = std::min(a.origin.y, p.y);
  Coord right = std::max(a.origin.x + a.size.width, p.x);
  Coord bottom = std::max(a.origin.y + a.size.height, p.y);
  return {
      {right - left, bottom - top},
      {        left,          top}
  };
}

inline Dimensions scaleToFit(const Dimensions& src, const Dimensions& dst) {
  auto scale = std::min(dst.width / src.width, dst.height / src.height);
  return {src.width * scale, src.height * scale};
}

inline Point translateInto(Point pos, const Box& src, const Box& dst) {
  auto scale = std::min(dst.size.width / src.size.width, dst.size.height / src.size.height);
  return {dst.origin.x + scale * (pos.x - src.origin.x), dst.origin.y + scale * (pos.y - src.origin.y)};
}

struct Transform {
  Box operator()(const Box& v) const { return {(*this)(v.size), (*this)(v.origin)}; }

  Dimensions operator()(const Dimensions& v) const {
    return {std::abs(matrix[0] * v.width + matrix[1] * v.height), std::abs(matrix[2] * v.width + matrix[3] * v.height)};
  }

  Point operator()(const Point& v) const {
    return {matrix[0] * v.x + matrix[1] * v.y + offset.x, matrix[2] * v.x + matrix[3] * v.y + offset.y};
  }

  Coord matrix[4];
  Point offset;
};

static constexpr Transform IDENTITY = {
    .matrix = {1, 0, 0, 1},
      .offset = {0, 0  }
};
static constexpr Transform ROTATE_LEFT = {
    .matrix = {0, 1, -1, 0},
      .offset = {0, 0   }
};
static constexpr Transform ROTATE_RIGHT = {
    .matrix = {0, -1, 1, 0},
      .offset = {0,  0  }
};
static constexpr Transform FLIP_HORIZ = {
    .matrix = {-1, 0, 0, 1},
      .offset = { 0, 0  }
};

template <typename T, typename R>
R rotateLeft(const T& v) {
  return transform(ROTATE_LEFT, v);
}

}  // namespace jazzlights
#endif  // JAZZLIGHTS_GEOM_H
