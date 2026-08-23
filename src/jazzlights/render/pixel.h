#ifndef JL_RENDER_PIXEL_H
#define JL_RENDER_PIXEL_H

#include <cstddef>

#include "jazzlights/layout/layout.h"
#include "jazzlights/render/renderer.h"
#include "jazzlights/util/geom.h"

namespace jazzlights {

struct Strand {
  const Layout& layout;
  Renderer& renderer;
  size_t index;
};

struct Pixel {
  const Strand* strand = nullptr;
  size_t strandIndex = 0;
  size_t cumulativeIndex = 0;
  Point coord = {0.0, 0.0};
};

}  // namespace jazzlights

#endif  // JL_RENDER_PIXEL_H
