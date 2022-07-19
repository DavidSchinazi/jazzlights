#include "unisparks/types.h"

#include <set>
#include <vector>

#include "unisparks/layout.hpp"
#include "unisparks/util/log.hpp"

namespace unisparks {
namespace {
constexpr size_t kSmallerGridSize = 100;
constexpr Coord kCoordEpsilon = 0.0000001;
}  // namespace

void XYIndexStore::IngestLayout(const Layout* layout) {
  layoutInfos_.push_back(LayoutInfo());
  LayoutInfo& li = layoutInfos_.back();
  li.layout = layout;
  li.xyIndices.reserve(layout->pixelCount());
}

void XYIndexStore::Finalize(const Box& viewport) {
  std::vector<Coord> xValues;
  std::vector<Coord> yValues;
  {
    auto almostLess = [](Coord a, Coord b) {
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
  useSmallerXGrid_ = xValues.size() > kSmallerGridSize;
  if (useSmallerXGrid_) {
    xValuesCount_ = kSmallerGridSize;
  } else {
    xValuesCount_ = xValues.size();
  }
  useSmallerYGrid_ = yValues.size() > kSmallerGridSize;
  if (useSmallerYGrid_) {
    yValuesCount_ = kSmallerGridSize;
  } else {
    yValuesCount_ = yValues.size();
  }
  for (LayoutInfo& li : layoutInfos_) {
    const int pixelCount = li.layout->pixelCount();
    for (int i = 0; i < pixelCount; i++) {
      const Point pt = li.layout->at(i);
      XYIndex xyIndex;
      if (useSmallerXGrid_) {
        xyIndex.xIndex = (pt.x - viewport.origin.x) * kSmallerGridSize / viewport.size.width;
      } else {
        for (size_t xi = 0; xi < xValuesCount_; xi++) {
          if (fabs(xValues[xi] - pt.x) < kCoordEpsilon) {
            xyIndex.xIndex = xi;
            break;
          }
        }
      }
      if (useSmallerYGrid_) {
        xyIndex.yIndex = (pt.y - viewport.origin.y) * kSmallerGridSize / viewport.size.height;
      } else {
        for (size_t yi = 0; yi < yValuesCount_; yi++) {
          if (fabs(yValues[yi] - pt.y) < kCoordEpsilon) {
            xyIndex.yIndex = yi;
            break;
          }
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