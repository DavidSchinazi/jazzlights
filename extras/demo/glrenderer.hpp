#ifndef UNISPARKS_RENDERERS_OPENGL_H
#define UNISPARKS_RENDERERS_OPENGL_H
#include "unisparks/renderer.hpp"
#include "unisparks/layout.hpp"
namespace unisparks {

class GLRenderer : public Renderer {
 public:
  GLRenderer(const Layout& l, Meters ledRadius = 1/120.0);

  void render(InputStream<Color>& pixelColors) override;

private:
  const Layout& layout_;
  Meters ledRadius_;
};


} // namespace unisparks
#endif /* UNISPARKS_RENDERERS_OPENGL_H */



