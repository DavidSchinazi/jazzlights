#ifndef JL_LAYOUT_MATRIX_H
#define JL_LAYOUT_MATRIX_H

#include "jazzlights/layout/layout.h"
namespace jazzlights {

using PixelsPerMeter = double;

class Matrix : public Layout {
 public:
  constexpr Matrix(size_t w, size_t h) : width_(w), height_(h) {}
  constexpr Matrix(size_t w, size_t h, PixelsPerMeter r) : width_(w), height_(h), resolution_(r) {}

  Matrix(const Matrix& other)
      : Layout(),
        width_(other.width_),
        height_(other.height_),
        origin_(other.origin_),
        resolution_(other.resolution_),
        flags_(other.flags_) {}

  Matrix& resolution(PixelsPerMeter r) {
    resolution_ = r;
    return *this;
  }

  Matrix& origin(Coord x, Coord y) {
    origin_ = {x, y};
    return *this;
  }

  Matrix& progressive() {
    flags_ |= PROGRESSIVE;
    return *this;
  }

  Matrix& zigzag() {
    flags_ |= ZIGZAG;
    return *this;
  }

  Dimensions size() const { return {width_ / resolution_, height_ / resolution_}; }

  size_t pixelCount() const override { return width_ * height_; }

  Point at(size_t index) const override {
    size_t x = (index % width_);
    size_t y = (index / width_);
    if ((flags_ & ZIGZAG) && y % 2 == 0) { x = width_ - x - 1; }
    return {origin_.x + x / resolution_, origin_.y + y / resolution_};
  }

 private:
  enum { PROGRESSIVE = 0, ZIGZAG = 1 };

  size_t width_;
  size_t height_;
  Point origin_ = {0, 0};
  PixelsPerMeter resolution_ = 60.0;
  uint32_t flags_ = 0;
};

}  // namespace jazzlights
#endif  // JL_LAYOUT_MATRIX_H
