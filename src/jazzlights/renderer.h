#ifndef JL_RENDERER_H
#define JL_RENDERER_H

#include "jazzlights/util/color.h"

namespace jazzlights {

class Renderer {
 public:
  virtual ~Renderer() = default;

  virtual void renderPixel(size_t index, Color color) = 0;
};

}  // namespace jazzlights
#endif  // JL_RENDERER_H
