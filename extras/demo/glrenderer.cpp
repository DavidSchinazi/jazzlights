#include "glrenderer.h"

#include <GLFW/glfw3.h>
#include <math.h>
namespace jazzlights {

static constexpr double kTwoPi = 2 * 3.1415926;

static void RenderLed(double cx, double cy, double r, CRGB color, int segments = 6) {
  glColor3f(color.red / 255.f, color.green / 255.f, color.blue / 255.f);
  glBegin(GL_TRIANGLE_FAN);
  glVertex2f(cx, cy);
  for (int i = 0; i <= segments; i++) {
    glVertex2f(cx + (r * cos(i * kTwoPi / segments)), cy + (r * sin(i * kTwoPi / segments)));
  }
  glEnd();
}

GLRenderer::GLRenderer(const Layout& layout, Meters ledRadius) : layout_(layout), ledRadius_(ledRadius) {}

void GLRenderer::RenderPixel(size_t index, CRGB color) {
  Point pos = layout_.At(index);
  RenderLed(pos.x, pos.y, ledRadius_, color);
}

}  // namespace jazzlights
