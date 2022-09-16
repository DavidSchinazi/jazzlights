#ifndef JAZZLIGHTS_LAYOUTS_REVERSEMAP_H
#define JAZZLIGHTS_LAYOUTS_REVERSEMAP_H
#include "jazzlights/layouts/pixelmap.hpp"

namespace jazzlights {

inline void makeReverseMap(Point* points, const int* map, int w, int h,
                           int count) {
  // zero-init points
  for (int i = 0; i < count; points[i++] = {});
  for (int x = 0; x < w; ++x) {
    for (int y = 0; y < h; ++y) {
      int i = map[x + y * w];
      if (0 <= i && i < count) {
        points[i] = {static_cast<Coord>(x), static_cast<Coord>(y)};
      }
    }
  }
}

template<size_t COUNT>
struct ReverseMap : public PixelMap {
  ReverseMap(const int* map, int w, int h) : PixelMap(COUNT, points) {
    makeReverseMap(points, map, w, h, COUNT);
  }
  Point points[COUNT];
};

}  // namespace jazzlights
#endif  // JAZZLIGHTS_LAYOUTS_REVERSEMAP_H

