#ifndef UNISPARKS_LAYOUTS_MATRIX_H
#define UNISPARKS_LAYOUTS_MATRIX_H
#include "unisparks/layout.hpp"
namespace unisparks {

struct Matrix : public Layout {
  enum {PROGRESSIVE = 0, ZIGZAG = 1};

  Matrix(int w, int h, int flags = 0) : Matrix(Box{{Coord(w), Coord(h)}, {0, 0}}, flags) {
  }

  Matrix(Coord l, Coord t, int w, int h, int flags = 0)
    : Matrix(Box{{Coord(w), Coord(h)}, {l, t}}, flags) {
  }

  Matrix(const Box& p, int flags = 0) : pos(p), flags(flags) {
  }

  Matrix(const Matrix& other) : pos(other.pos), flags(other.flags) {
  }

  Box bounds() const override {
    return pos;
  }

  int pixelCount() const override {
    return pos.size.width * pos.size.height;
  }

  Point at(int index) const override {
    int x = (index % static_cast<int>(pos.size.width));
    int y = (index / static_cast<int>(pos.size.width));
    if ((flags & ZIGZAG) && y % 2 == 0) {
      x = pos.size.width - x - 1;
    }
    return {pos.origin.x + x, pos.origin.y + y};
  }

  Box pos;
  uint32_t flags = 0;
};

} // namespace unisparks
#endif /* UNISPARKS_LAYOUTS_MATRIX_H */
