#ifndef JAZZLIGHTS_RENDERERS_OPENGL_H
#define JAZZLIGHTS_RENDERERS_OPENGL_H

#include "jazzlights/layout.h"
#include "jazzlights/renderer.h"

namespace jazzlights {

class GLRenderer : public Renderer {
 public:
  explicit GLRenderer(const Layout& l, Meters ledRadius = 1.0 / 60.0);

  void render(InputStream<Color>& pixelColors) override;

 private:
  const Layout& layout_;
  Meters ledRadius_;
};

}  // namespace jazzlights

#endif  // JAZZLIGHTS_RENDERERS_OPENGL_H
