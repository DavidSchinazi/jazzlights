#ifndef JL_TYPES_H
#define JL_TYPES_H

#include <cstdint>
#include <vector>

#include "jazzlights/util/geom.h"

namespace jazzlights {

using PatternBits = uint32_t;
using Precedence = uint16_t;
using NumHops = uint8_t;

// bitNum is [1-32] starting from the highest bit.
constexpr bool patternbit(PatternBits pattern, uint8_t bitNum) {
  return (pattern & (1 << (sizeof(PatternBits) * 8 - bitNum))) != 0;
}

PatternBits randomizePatternBits(PatternBits pattern);

class Layout;

struct Pixel {
  const Layout* layout = nullptr;
  size_t index = 0;
  Point coord;
};

struct XYIndex {
  size_t xIndex = 0;
  size_t yIndex = 0;
};

class XYIndexStore {
 public:
  XYIndexStore();
  void Reset();
  void IngestLayout(const Layout* layout);
  void Finalize(const Box& viewport);
  XYIndex FromPixel(const Pixel& pixel) const;
  size_t xValuesCount() const { return xValuesCount_; }
  size_t yValuesCount() const { return yValuesCount_; }

 private:
  struct LayoutInfo {
    const Layout* layout;
    std::vector<XYIndex> xyIndices;
  };
  std::vector<LayoutInfo> layoutInfos_;
  size_t xValuesCount_;
  size_t yValuesCount_;
  bool useSmallerXGrid_;
  bool useSmallerYGrid_;
};

}  // namespace jazzlights

#endif  // JL_TYPES_H
