#ifndef JL_UTIL_GEOM_H
#define JL_UTIL_GEOM_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace jazzlights {

constexpr double Square(double x) { return x * x; }

using Coord = double;
using Meters = double;

struct Vector2D {
  Coord x;
  Coord y;
};

using Point2D = Vector2D;
using Point = Point2D;

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

constexpr Coord Left(const Box& b) { return b.origin.x; }

constexpr Coord Top(const Box& b) { return b.origin.y; }

constexpr Coord Right(const Box& b) { return b.origin.x + b.size.width; }

constexpr Coord Bottom(const Box& b) { return b.origin.y + b.size.height; }

constexpr Coord Width(const Box& b) { return b.size.width; }

constexpr Coord Height(const Box& b) { return b.size.height; }

constexpr Point Center(Box b) { return {b.origin.x + b.size.width / 2, b.origin.y + b.size.height / 2}; }

constexpr Point operator+(const Point& a, const Point& b) { return {a.x + b.x, a.y + b.y}; }

constexpr Point operator*(double k, const Point& v) { return {Coord(k * v.x), Coord(k * v.y)}; }

// The functions below are inline instead of constexpr because we need to support C++11,
// and std::min/max became constexpr in C++14, and sqrt became constexpr in C++26.

inline double Distance(Point a, Point b) { return sqrt(Square(a.x - b.x) + Square(a.y - b.y)); }

inline Box Merge(const Box& a, const Box& b) {
  Coord left = std::min(a.origin.x, b.origin.x);
  Coord top = std::min(a.origin.y, b.origin.y);
  Coord right = std::max(a.origin.x + a.size.width, b.origin.x + b.size.width);
  Coord bottom = std::max(a.origin.y + a.size.height, b.origin.y + b.size.height);
  return {
      {right - left, bottom - top},
      {        left,          top}
  };
}

inline Box Merge(const Box& a, const Point& p) {
  Coord left = std::min(a.origin.x, p.x);
  Coord top = std::min(a.origin.y, p.y);
  Coord right = std::max(a.origin.x + a.size.width, p.x);
  Coord bottom = std::max(a.origin.y + a.size.height, p.y);
  return {
      {right - left, bottom - top},
      {        left,          top}
  };
}

}  // namespace jazzlights
#endif  // JL_UTIL_GEOM_H
