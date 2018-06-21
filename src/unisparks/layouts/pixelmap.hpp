#ifndef UNISPARKS_LAYOUTS_PIXELMAP_H
#define UNISPARKS_LAYOUTS_PIXELMAP_H
#include "unisparks/layout.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

class PixelMap : public Layout {
public:
  PixelMap(size_t cnt, Point* pts) : count_(cnt), points_(pts) {
  }

  PixelMap(const PixelMap& other) : count_(other.count_), points_(other.points_) {
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

} // namespace unisparks
#endif /* UNISPARKS_LAYOUTS_PIXELMAP_H */
