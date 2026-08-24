#ifndef JL_EXTRAS_DEMO_GLRENDERER_H
#define JL_EXTRAS_DEMO_GLRENDERER_H

#include "jazzlights/layout/layout.h"
#include "jazzlights/render/renderer.h"

namespace jazzlights {

class GLRenderer : public Renderer {
 public:
  explicit GLRenderer(const Layout& layout, Meters ledRadius = 1.0 / 60.0);

  void RenderPixel(size_t index, CRGB color) override;

 private:
  const Layout& layout_;
  Meters ledRadius_;
};

}  // namespace jazzlights

#endif  // JL_EXTRAS_DEMO_GLRENDERER_H
