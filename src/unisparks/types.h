#ifndef UNISPARKS_TYPES_H
#define UNISPARKS_TYPES_H

#include <cstdint>
#include <vector>

#include "unisparks/util/geom.hpp"

namespace unisparks {

using PatternBits = uint32_t;
using Precedence = uint16_t;
using NumHops = uint8_t;

// bitNum is [1-32] starting from the highest bit.
constexpr inline bool patternbit(PatternBits pattern, uint8_t bitNum) {
  return (pattern & (1 << (sizeof(PatternBits) * 8 - bitNum))) != 0;
}

class Layout;

struct Pixel {
  const Layout* layout = nullptr;
  int index = 0;
  Point coord;
};

struct XYIndex {
  int xIndex = 0;
  int yIndex = 0;
};

class XYIndexStore {
 public:
  XYIndexStore();
  void Reset();
  void IngestLayout(const Layout* layout);
  void Finalize();
  XYIndex FromPixel(const Pixel& pixel) const;
  int xValuesCount() const { return xValuesCount_; }
  int yValuesCount() const { return yValuesCount_; }
 private:
  struct LayoutInfo {
    const Layout* layout;
    std::vector<XYIndex> xyIndices;
  };
  std::vector<LayoutInfo> layoutInfos_;
  int xValuesCount_;
  int yValuesCount_;
};

} // namespace unisparks

#endif // UNISPARKS_TYPES_H
