#ifndef UNISPARKS_GEOM_H
#define UNISPARKS_GEOM_H
#include "unisparks/util/math.hpp"

namespace unisparks {

// I originally tried integer math, but it seems like it doesn't make 
// much difference neither on PC nor on ESP8266 board.
//
// Most likely this is because the main performance hog by far is conversion 
// between RGB & HSL spaces, which I do in floating-point anyway. And I
// do it this way because everything I tried with fixed point so far looked 
// like crap. I may try again if I figure out fast AND decent looking HSL/RGBA 
// transform...
using Coord = double;

struct Vector2D {
  Coord x;
  Coord y;
};

using Point2D = Vector2D;
using Point = Point2D;
using Origin = Point2D;

struct Dimensions {
  Coord width;
  Coord height;
};

struct Box {
  Dimensions size;
  Point origin;
};

constexpr Point center(Box b) {
  return {b.origin.x + b.size.width/2, b.origin.y + b.size.height/2};
}

inline Point operator+(const Point& a, const Point& b) {
  return {a.x + b.x, a.y + b.y};
}

inline Point operator*(double k, const Point& v) {
  return {Coord(k * v.x), Coord(k * v.y)};
}

inline double distance(Point a, Point b) {
  return sqrt(square(a.x - b.x) + square(a.y - b.y));
}

struct Transform {

  Box operator()(const Box& v) const { return {
    (*this)(v.size),
    (*this)(v.origin)};
  }

  Dimensions operator()(const Dimensions& v) const {
    return { matrix[0]* v.width + matrix[1]* v.height,
             matrix[2]* v.width + matrix[3]* v.height};
  }

  Point operator()(const Point& v) const {
    return { matrix[0]* v.x + matrix[1]* v.y + offset.x,
             matrix[2]* v.x + matrix[3]* v.y + offset.y};
  }

  int matrix[4];
  Point offset;
};

static constexpr Transform IDENTITY = {.matrix = {1, 0, 0, 1}, .offset = {0, 0}};
static constexpr Transform ROTATE_LEFT = {.matrix = {0, 1, -1, 0}, .offset = {0, 0}};
static constexpr Transform ROTATE_RIGHT = {.matrix = {0, -1, 1, 0}, .offset = {0, 0}};
static constexpr Transform FLIP_HORIZ = {.matrix = {-1, 0, 0, 1}, .offset = {0, 0}};

} // namespace unisparks
#endif /* UNISPARKS_GEOM_H */
