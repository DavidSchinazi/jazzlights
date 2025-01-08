#ifndef JL_LAYOUT_LAYOUT_H
#define JL_LAYOUT_LAYOUT_H

#include "jazzlights/types.h"
#include "jazzlights/util/geom.h"
#include "jazzlights/util/math.h"

namespace jazzlights {

class Layout {
 public:
  Layout() = default;
  virtual ~Layout() = default;
  // Disallow copy and move.
  Layout(const Layout& other) = delete;
  Layout(Layout&& other) = delete;
  Layout& operator=(const Layout& other) = delete;
  Layout& operator=(Layout&& other) = delete;

  virtual size_t pixelCount() const = 0;
  virtual Point at(size_t i) const = 0;
};

inline Box bounds(const Layout& layout) {
  Box bb;
  const size_t numPixels = layout.pixelCount();
  for (size_t index = 0; index < numPixels; index++) { bb = merge(bb, layout.at(index)); }
  return bb;
}

}  // namespace jazzlights

#endif  // JL_LAYOUT_LAYOUT_H
