#ifndef UNISPARKS_RENDERERS_FUNCTIONAL_H
#define UNISPARKS_RENDERERS_FUNCTIONAL_H
#include "unisparks/renderer.hpp"

namespace unisparks {

using SimpleRenderFunc = void (*)(int i, uint8_t r, uint8_t g, uint8_t b);

class SimpleRenderer : public Renderer {
 public:
  SimpleRenderer() = default;
  SimpleRenderer(SimpleRenderFunc f) : render_(f) {
  }
  SimpleRenderer(const SimpleRenderer& other) : render_(other.render_) {
  }

  void render(InputStream<Color>& pixelColors, size_t) override {
    if (!render_) {
      return;
    }
    int i=0;
    for (auto color : pixelColors) {
      auto rgba = color.asRgba();
      render_(i++, rgba.red, rgba.green, rgba.blue);
    }
  }

 private:
  SimpleRenderFunc render_ = nullptr;
};

} // namespace unisparks
#endif /* UNISPARKS_RENDERERS_FUNCTIONAL_H */
