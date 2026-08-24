#ifndef JL_LAYOUT_PIXELMAP_H
#define JL_LAYOUT_PIXELMAP_H

#include "jazzlights/layout/layout.h"

namespace jazzlights {

class PixelMap : public Layout {
 public:
  PixelMap(size_t cnt, const Point* pts) : count_(cnt), points_(pts) {}

  PixelMap(const PixelMap& other) : Layout(), count_(other.count_), points_(other.points_) {}

  size_t PixelCount() const override { return count_; }

  Point At(size_t i) const override { return points_[i]; }

 private:
  size_t count_;
  const Point* points_;
};

}  // namespace jazzlights
#endif  // JL_LAYOUT_PIXELMAP_H
