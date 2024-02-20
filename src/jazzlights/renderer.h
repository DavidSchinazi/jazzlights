#ifndef JL_RENDERER_H
#define JL_RENDERER_H

#include "jazzlights/fastled_wrapper.h"

namespace jazzlights {

class Renderer {
 public:
  virtual ~Renderer() = default;

  virtual void renderPixel(size_t index, CRGB color) = 0;
};

}  // namespace jazzlights
#endif  // JL_RENDERER_H
