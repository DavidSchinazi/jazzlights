#include "DFSparks.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <cmath>
#include <mutex>
#include <stdio.h>

namespace dfsparks {
namespace ui {

const double TWO_PI = 2 * 3.1415926;
const double LED_R = 0.03; // in window heights
const double LED_DIST = 3 * LED_R;

int32_t millis() {
  static auto t0 = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - t0)
      .count();
}

void render_circle(double cx, double cy, double r, uint32_t color) {
  const int segments = 100;
  glColor4f(chan_red(color) / 255.f, chan_green(color) / 255.f,
            chan_blue(color) / 255.f, chan_alpha(color) / 255.f);
  glEnable(GL_LINE_SMOOTH);
  glBegin(GL_LINES);
  for (int i = 0; i <= segments; ++i) {
    glVertex2f(cx, cy);
    glVertex2f(cx + (r * cos(i * TWO_PI / segments)),
               cy + (r * sin(i * TWO_PI / segments)));
  }
  glEnd();
}

void render_led(double cx, double cy, uint32_t color) {
  render_circle(cx, cy, LED_R, color);
}

class Vest : public UdpSocketPlayer {
public:
  enum { width = 8, height = 17 }; 
  Vest(double x, double y) : x0(x), y0(y) {}
  ~Vest() {}

private:
  int on_get_number_of_pixels() const override { return width * height; };
  int on_get_width() const override{ return width; }
  int on_get_height() const override { return height; }
  void on_get_coords(int i, int *x, int *y) const override { *x = i / height; *y = i % height; }
  void on_set_pixel_color(int index, uint8_t red, uint8_t green,
                                  uint8_t blue, uint8_t alpha) override {
    int x,y;
    on_get_coords(index, &x, &y);
    render_led(x0 + 2.5 * LED_R * x, y0 + 2.5 * LED_R * y,
               rgba(red, green, blue, alpha));
  }

  double x0;
  double y0;
};

class Ring : public UdpSocketPlayer {
public:
  Ring(int length, double cx, double cy)
      : length(length), R(6*length/ TWO_PI), r(LED_DIST * length / TWO_PI), cx(cx), cy(cy) {}
  ~Ring() {}

private:
  int on_get_number_of_pixels() const override { return length; };
  int on_get_width() const override{ return 2*R; }
  int on_get_height() const override { return 2*R; }
  void on_get_coords(int i, int *x, int *y) const override { 
      *x = R + int(R * cos(i * TWO_PI / length));
      *y = R + int(R * sin(i * TWO_PI / length));
  }
  void on_set_pixel_color(int i, uint8_t red, uint8_t green,
                                  uint8_t blue, uint8_t alpha) override {
    double x = r * cos(i * TWO_PI / length);
    double y = r * sin(i * TWO_PI / length);
    render_led(cx + x, cy + y, rgba(red, green, blue, alpha));
  }

  int length;
  int R;
  double r;
  double cx;
  double cy;
};

Vest vest1(-0.7, -0.5);
Ring ring1(12, 0.2, 0.5);

void on_resize(GLFWwindow *window, int width, int height) {
  double aspect = 1.0 * width / height;
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-aspect, aspect, -1, 1, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

void on_key(GLFWwindow *window, int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_LEFT && action == GLFW_PRESS) {
    vest1.prev();
  }
  if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) {
    vest1.next();
  }
  if (key == GLFW_KEY_S && action == GLFW_PRESS) {
    vest1.end();
    vest1.begin(NetworkPlayer::server);
  }
  if (key == GLFW_KEY_C && action == GLFW_PRESS) {
    vest1.end();
    vest1.begin(NetworkPlayer::offline);
  }
}

int run() {
  const int WIN_W = 600;
  const int WIN_H = 480;

  GLFWwindow *window;
  if (!glfwInit())
    return -1;

  window = glfwCreateWindow(WIN_W, WIN_H, "DiscoFish Sparks Demo", NULL, NULL);
  if (!window) {
    glfwTerminate();
    return -1;
  }
  glfwSetWindowSizeCallback(window, on_resize);
  glfwMakeContextCurrent(window);
  glfwSetKeyCallback(window, on_key);

  on_resize(window, WIN_W, WIN_H);

  vest1.begin(NetworkPlayer::server);
  ring1.begin(NetworkPlayer::client);

  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT);

    vest1.render();
    ring1.render();

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();
  return 0;
}

} // namespace ui
} // namespace dfsparks
