#ifndef JAZZLIGHTS_LAYOUTS_PIXELMAP_H
#define JAZZLIGHTS_LAYOUTS_PIXELMAP_H
#include "jazzlights/layout.hpp"
#include "jazzlights/util/math.hpp"

namespace jazzlights {

class PixelMap : public Layout {
public:
  PixelMap(size_t cnt, const Point* pts) : count_(cnt), points_(pts) {
  }

  PixelMap(const PixelMap& other) : Layout(), count_(other.count_), points_(other.points_) {
  }

  int pixelCount() const override {
    return count_;
  }
  
  Point at(int i) const override {
    return points_[i];
  }

private:
  size_t count_;
  const Point* points_;
};

}  // namespace jazzlights
#endif  // JAZZLIGHTS_LAYOUTS_PIXELMAP_H
