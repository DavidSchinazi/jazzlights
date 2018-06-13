#ifndef UNISPARKS_LAYOUTS_PIXELMAP_H
#define UNISPARKS_LAYOUTS_PIXELMAP_H
#include "unisparks/layout.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

class PixelMap : public Layout {
public:
  PixelMap(size_t cnt, Point* pts) : PixelMap(cnt, pts, true) {
  }

  int pixelCount() const override {
    return count_;
  }
  
  Box bounds() const override {
    return bounds_;
  }
  
  Point at(int i) const override {
    return points_[i];
  }

protected:
  PixelMap(size_t cnt, Point* pts, bool calcb) : count_(cnt), points_(pts) {
    if (calcb) {
      bounds_ = calculateBounds();
    }
  }
  Box bounds_;

private:
  size_t count_;
  const Point* points_;
};

} // namespace unisparks
#endif /* UNISPARKS_LAYOUTS_PIXELMAP_H */
