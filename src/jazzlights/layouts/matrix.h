#ifndef JL_LAYOUTS_MATRIX_H
#define JL_LAYOUTS_MATRIX_H
#include "jazzlights/layouts/layout.h"
namespace jazzlights {

using PixelsPerMeter = double;

class Matrix : public Layout {
 public:
  constexpr Matrix(int w, int h) : width_(w), height_(h) {}

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

  int pixelCount() const override { return width_ * height_; }

  Point at(int index) const override {
    int x = (index % width_);
    int y = (index / width_);
    if ((flags_ & ZIGZAG) && y % 2 == 0) { x = width_ - x - 1; }
    return {origin_.x + x / resolution_, origin_.y + y / resolution_};
  }

 private:
  enum { PROGRESSIVE = 0, ZIGZAG = 1 };

  int width_;
  int height_;
  Point origin_ = {0, 0};
  PixelsPerMeter resolution_ = 60.0;
  uint32_t flags_ = 0;
};

}  // namespace jazzlights
#endif  // JL_LAYOUTS_MATRIX_H
