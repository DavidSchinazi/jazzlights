#include "jazzlights/types.h"

#include <set>
#include <vector>

#include "jazzlights/layout.h"
#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/log.h"

namespace jazzlights {
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
    auto almostLess = [](Coord a, Coord b) { return a + kCoordEpsilon < b; };
    std::set<Coord, decltype(almostLess)> xSet(almostLess);
    std::set<Coord, decltype(almostLess)> ySet(almostLess);
    for (LayoutInfo& li : layoutInfos_) {
      const size_t pixelCount = li.layout->pixelCount();
      for (size_t i = 0; i < pixelCount; i++) {
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
    const size_t pixelCount = li.layout->pixelCount();
    for (size_t i = 0; i < pixelCount; i++) {
      const Point pt = li.layout->at(i);
      XYIndex xyIndex;
      if (useSmallerXGrid_) {
        xyIndex.xIndex = (pt.x - viewport.origin.x) * kSmallerGridSize / viewport.size.width;
        if (xyIndex.xIndex == xValuesCount_) { xyIndex.xIndex--; }
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
        if (xyIndex.yIndex == yValuesCount_) { xyIndex.yIndex--; }
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
    if (li.layout == pixel.layout) { return li.xyIndices[pixel.index]; }
  }
  return XYIndex();
}

XYIndexStore::XYIndexStore() { Reset(); }

void XYIndexStore::Reset() {
  layoutInfos_.clear();
  xValuesCount_ = 0;
  yValuesCount_ = 0;
}

PatternBits randomizePatternBits(PatternBits pattern) {
  bool reservedWithPalette = false;
  if ((pattern & 0xF) == 0) {  // Pattern is reserved.
    if ((pattern & 0xFF) == 0x30) {
      reservedWithPalette = true;
    } else {
      // Don't randomize reserved patterns unless they're reserved with palette.
      return pattern;
    }
  }
  pattern &= 0xC000E000;  // Clear bits 13-15 for palette and 30-31 for pattern.
  if (reservedWithPalette) {
    pattern |= 0x30;
  } else {
    pattern |= UnpredictableRandom::GetNumberBetween(1, (1 << 4) - 1);  // Randomize bits 0-3, but must not be all-zero.
    pattern |= UnpredictableRandom::GetNumberBetween(0, (1 << 9) - 1) << 4;  // Randomize bits 4-12.
  }
  pattern |= UnpredictableRandom::GetNumberBetween(0, (1 << 14) - 1) << 16;  // Randomize bits 16-29.
  return pattern;
}

}  // namespace jazzlights
