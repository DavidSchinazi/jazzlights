#ifndef JL_RENDER_XY_INDEX_H
#define JL_RENDER_XY_INDEX_H

#include <vector>

#include "jazzlights/types.h"
#include "jazzlights/util/geom.h"

namespace jazzlights {

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

#endif  // JL_RENDER_XY_INDEX_H
