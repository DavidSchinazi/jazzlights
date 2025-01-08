#ifndef JL_LAYOUTS_PIXELMAP_H
#define JL_LAYOUTS_PIXELMAP_H

#include "jazzlights/layouts/layout.h"
#include "jazzlights/util/math.h"

namespace jazzlights {

class PixelMap : public Layout {
 public:
  PixelMap(size_t cnt, const Point* pts) : count_(cnt), points_(pts) {}

  PixelMap(const PixelMap& other) : Layout(), count_(other.count_), points_(other.points_) {}

  size_t pixelCount() const override { return count_; }

  Point at(size_t i) const override { return points_[i]; }

 private:
  size_t count_;
  const Point* points_;
};

}  // namespace jazzlights
#endif  // JL_LAYOUTS_PIXELMAP_H
