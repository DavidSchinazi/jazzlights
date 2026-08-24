#ifndef JL_RENDER_RENDERER_H
#define JL_RENDER_RENDERER_H

#include "jazzlights/render/fastled_wrapper.h"

namespace jazzlights {

class Renderer {
 public:
  virtual ~Renderer() = default;

  virtual void RenderPixel(size_t index, CRGB color) = 0;
};

}  // namespace jazzlights
#endif  // JL_RENDER_RENDERER_H
