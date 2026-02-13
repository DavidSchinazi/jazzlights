#ifndef JL_TYPES_H
#define JL_TYPES_H

#include <cstdint>
#include <limits>
#include <vector>

#include "jazzlights/config.h"
#include "jazzlights/util/geom.h"

namespace jazzlights {

// We would normally define our int types using fixed-width types such as as int32_t, but then some compilers would
// require us to use PRId32 as a printf format, so we define them as older types so we can use %u or %d instead.

#define JL_ASSERT_INT_TYPES_EQUAL(_a, _b)                                                                       \
  static_assert(sizeof(_a) == sizeof(_b) && std::numeric_limits<_a>::min() == std::numeric_limits<_b>::min() && \
                    std::numeric_limits<_a>::max() == std::numeric_limits<_b>::max(),                           \
                "bad int type")

#define JL_DEFINE_INT_TYPE(_new_type, _base_type, _expected_type) \
  using _new_type = _base_type;                                   \
  JL_ASSERT_INT_TYPES_EQUAL(_new_type, _expected_type)

JL_DEFINE_INT_TYPE(PatternBits, unsigned int, uint32_t);
using Precedence = uint16_t;
using NumHops = uint8_t;

// bitNum is [1-32] starting from the highest bit.
constexpr bool patternbit(PatternBits pattern, uint8_t bitNum) {
  return (pattern & (1 << (sizeof(PatternBits) * 8 - bitNum))) != 0;
}

PatternBits randomizePatternBits(PatternBits pattern);

constexpr Precedence OverridePrecedence() { return 36000; }

class Layout;

struct Pixel {
  const Layout* layout = nullptr;
  size_t index = 0;
  Point coord = {0.0, 0.0};
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
