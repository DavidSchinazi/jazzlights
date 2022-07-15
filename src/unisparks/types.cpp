#include "unisparks/types.h"

#include <set>
#include <vector>

#include "unisparks/layout.hpp"
#include "unisparks/util/log.hpp"

namespace unisparks {

void XYIndexStore::IngestLayout(const Layout* layout) {
  layoutInfos_.push_back(LayoutInfo());
  LayoutInfo& li = layoutInfos_.back();
  li.layout = layout;
  li.xyIndices.reserve(layout->pixelCount());
}

void XYIndexStore::Finalize() {
  std::vector<Coord> xValues;
  std::vector<Coord> yValues;
  {
    auto almostLess = [](Coord a, Coord b) {
      static constexpr Coord kCoordEpsilon = 0.00001;
      return a + kCoordEpsilon < b;
    };
    std::set<Coord, decltype(almostLess)> xSet(almostLess);
    std::set<Coord, decltype(almostLess)> ySet(almostLess);
    for (LayoutInfo& li : layoutInfos_) {
      const int pixelCount = li.layout->pixelCount();
      for (int i = 0; i < pixelCount; i++) {
        const Point pt = li.layout->at(i);
        xSet.insert(pt.x);
        ySet.insert(pt.y);
      }
    }
    xValues.assign(xSet.begin(), xSet.end());
    yValues.assign(ySet.begin(), ySet.end());
  }
  xValuesCount_ = xValues.size();
  yValuesCount_ = yValues.size();
  for (LayoutInfo& li : layoutInfos_) {
    const int pixelCount = li.layout->pixelCount();
    for (int i = 0; i < pixelCount; i++) {
      const Point pt = li.layout->at(i);
      XYIndex xyIndex;
      for (int xi = 0; xi < xValuesCount_; xi++) {
        if (xValues[xi] == pt.x) {
          xyIndex.xIndex = xi;
          break;
        }
      }
      for (int yi = 0; yi < yValuesCount_; yi++) {
        if (yValues[yi] == pt.y) {
          xyIndex.yIndex = yi;
          break;
        }
      }
      li.xyIndices[i] = xyIndex;
    }
  }
}

XYIndex XYIndexStore::FromPixel(const Pixel& pixel) const {
  for (const LayoutInfo& li : layoutInfos_) {
    if (li.layout == pixel.layout) {
      return li.xyIndices[pixel.index];
    }
  }
  return XYIndex();
}

XYIndexStore::XYIndexStore() {
  Reset();
}

void XYIndexStore::Reset() {
  layoutInfos_.clear();
  xValuesCount_ = 0;
  yValuesCount_ = 0;
}

} // namespace unisparks