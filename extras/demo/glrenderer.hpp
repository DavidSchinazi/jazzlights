#ifndef JAZZLIGHTS_RENDERERS_OPENGL_H
#define JAZZLIGHTS_RENDERERS_OPENGL_H

#include "jazzlights/renderer.hpp"
#include "jazzlights/layout.hpp"

namespace jazzlights {

class GLRenderer : public Renderer {
 public:
  GLRenderer(const Layout& l, Meters ledRadius = 1/120.0);

  void render(InputStream<Color>& pixelColors) override;

private:
  const Layout& layout_;
  Meters ledRadius_;
};

}  // namespace jazzlights

#endif  // JAZZLIGHTS_RENDERERS_OPENGL_H
