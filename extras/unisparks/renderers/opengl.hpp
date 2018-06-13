#ifndef UNISPARKS_RENDERERS_OPENGL_H
#define UNISPARKS_RENDERERS_OPENGL_H
#include <GLFW/glfw3.h>
#include <math.h>

namespace unisparks {


class GLRenderer : public Renderer {
 public:
  GLRenderer(double vpl, double vpt, double vpw, double vph, const Layout& l) :
    viewportLeft(vpl),
    viewportTop(vpt),
    viewportWidth(vpw),
    viewportHeight(vph),
    layout(l) {
  }

  void render(InputStream<Color>& pixelColors, size_t pixelCount) override {
    double led_sp = fmin(viewportWidth / layout.bounds().size.width,
                         viewportHeight / layout.bounds().size.height);
    auto pti = layout.begin();
    auto cli = pixelColors.begin();
    while (cli != pixelColors.end()) {
      auto pos = *pti;
      auto color = *cli;
      renderCircle(viewportLeft +
                   (viewportWidth - layout.bounds().size.width * led_sp) / 2 + led_sp *
                   (pos.x + 0.5 - layout.bounds().origin.x),
                   viewportTop + led_sp * (pos.y + 0.5 - layout.bounds().origin.y), 0.4 * led_sp,
                   color.asRgba());

      ++pti;
      ++cli;
    }
  }

  static constexpr double TWO_PI = 2 * 3.1415926;

  static void renderCircle(double cx, double cy, double r,
                           unisparks::RgbaColor color,
                           int segments = 6) {

    glColor4f(color.red / 255.f, color.green / 255.f, color.blue / 255.f,
              color.alpha / 255.f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segments; i++) {
      glVertex2f(cx + (r * cos(i * TWO_PI / segments)),
                 cy + (r * sin(i * TWO_PI / segments)));
    }
    glEnd();
  }

  // static void add(unisparks::Player& p, unisparks::Layout& lo, double vpl, double vpt, double vpw, double vph) {
  //   p.add(lo, unisparks::make<GLRenderer>(vpl, vpt, vpw, vph, lo));
  // }

  double viewportLeft;
  double viewportTop;
  double viewportWidth;
  double viewportHeight;
  const Layout& layout;
};


} // namespace unisparks
#endif /* UNISPARKS_RENDERERS_OPENGL_H */

