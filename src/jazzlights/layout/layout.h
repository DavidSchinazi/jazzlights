#ifndef JL_LAYOUT_LAYOUT_H
#define JL_LAYOUT_LAYOUT_H

#include "jazzlights/util/geom.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/types.h"

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

  virtual size_t PixelCount() const = 0;
  virtual Point At(size_t i) const = 0;
};

inline Box Bounds(const Layout& layout) {
  Box bb = {
      {0.0, 0.0},
      {0.0, 0.0}
  };
  const size_t numPixels = layout.PixelCount();
  for (size_t index = 0; index < numPixels; index++) { bb = Merge(bb, layout.At(index)); }
  return bb;
}

}  // namespace jazzlights

#endif  // JL_LAYOUT_LAYOUT_H
