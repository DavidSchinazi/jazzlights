#include "dfsparks/log.h"
#include "dfsparks/player.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdio.h>
#include <vector>

const char *WIN_TITLE = "DiscoFish Sparks Demo";
const int WIN_W = 800;
const int WIN_H = 600;

const double TWO_PI = 2 * 3.1415926;
const double LED_R = 0.03; // in window heights

void render_circle(double cx, double cy, double r, uint32_t color,
                   int segments = 25) {
  using namespace dfsparks;

  glColor4f(chan_red(color) / 255.f, chan_green(color) / 255.f,
            chan_blue(color) / 255.f, chan_alpha(color) / 255.f);
  glBegin(GL_TRIANGLE_FAN);
  glVertex2f(cx, cy);
  for (int i = 0; i <= segments; i++) {
    glVertex2f(cx + (r * cos(i * TWO_PI / segments)),
               cy + (r * sin(i * TWO_PI / segments)));
  }
  glEnd();
}

void render_led(double x, double y, uint32_t color) {
  render_circle(x, y, LED_R, color);
}

dfsparks::UdpSocketNetwork network;

class Matrix : public dfsparks::NetworkPlayer {
public:
  Matrix(int w, int h, double vpl, double vpt, double vpw, double vph)
      : NetworkPlayer(pixels, network), pixels(w, h, vpl, vpt, vpw, vph) {}

private:
  struct Pixels : dfsparks::Matrix {
    Pixels(int w, int h, double vpl, double vpt, double vpw, double vph)
        : Matrix(w, h), viewport_left(vpl), viewport_top(vpt),
          viewport_width(vpw), viewport_height(vph) {
      led_sp = fmin(viewport_width / w, viewport_height / h);
    }
    void on_set_color(int x, int y, uint32_t color) final {
      render_circle(viewport_left + led_sp * (x + 0.5),
                    viewport_top + led_sp * (y + 0.5), 0.4 * led_sp, color);
    };

    double led_sp;
    double viewport_left;
    double viewport_top;
    double viewport_width;
    double viewport_height;
  } pixels;
};

template <typename StrandT> class StrandRing : public dfsparks::NetworkPlayer {
public:
  StrandRing(int length, double vpl, double vpt, double vpw, double vph)
      : NetworkPlayer(pixels, network), pixels(length, vpl, vpt, vpw, vph) {}

private:
  struct Pixels : public StrandT {
    Pixels(int length, double vpl, double vpt, double vpw, double vph)
        : StrandT(length), cx(vpl + vpw / 2), cy(vpt + vph / 2),
          r(0.8 * fmin(vpw, vph) / 2), led_sp(0.5 * TWO_PI * r / length) {}

    void on_set_color(int i, uint32_t color) final {
      int length = StrandT::get_length();
      double x = r * cos(i * TWO_PI / length);
      double y = r * sin(i * TWO_PI / length);
      render_circle(cx + x, cy + y, 0.8 * led_sp, color);
    };

    double cx;
    double cy;
    double r;
    double led_sp;
  } pixels;
};

class TrueRing : public dfsparks::NetworkPlayer {
public:
  TrueRing(int length, double vpl, double vpt, double vpw, double vph)
      : NetworkPlayer(pixels, network), pixels(length, vpl, vpt, vpw, vph) {}

private:
  struct Pixels : public dfsparks::Pixels {
    Pixels(int length, double vpl, double vpt, double vpw, double vph)
        : length(length), R(6 * length / TWO_PI), cx(vpl + vpw / 2),
          cy(vpt + vph / 2), r(0.8 * fmin(vpw, vph) / 2),
          led_sp(0.5 * TWO_PI * r / length) {}

    int on_get_number_of_pixels() const final { return length; };
    int on_get_width() const final { return 2 * R; }
    int on_get_height() const final { return 2 * R; }
    void on_get_coords(int i, int *x, int *y) const final {
      *x = R + int(R * cos(i * TWO_PI / length));
      *y = R + int(R * sin(i * TWO_PI / length));
    }
    void on_set_color(int i, uint32_t color) final {
      double x = r * cos(i * TWO_PI / length);
      double y = r * sin(i * TWO_PI / length);
      render_circle(cx + x, cy + y, 0.8 * led_sp, color);
    };

    int length;
    int R;
    double cx;
    double cy;
    double r;
    double led_sp;
  } pixels;
};

std::vector<std::shared_ptr<dfsparks::NetworkPlayer>> sparks = {
    std::make_shared<Matrix>(250, 50, 0.0, 0.0, 800.0, 160.0),
    std::make_shared<Matrix>(8, 17, 40.0, 170.0, 160.0, 440.0),
    std::make_shared<StrandRing<dfsparks::HorizontalStrand>>(12, 250.0, 200.0, 90.0, 90.0),
    std::make_shared<StrandRing<dfsparks::VerticalStrand>>(12, 350.0, 200.0, 90.0, 90.0),
    std::make_shared<TrueRing>(12, 450.0, 200.0, 90.0, 90.0)};

auto &controlled = *sparks[0];

void on_resize(GLFWwindow * /*window*/, int width, int height) {
  double aspect = 1.0 * width * WIN_H / (height * WIN_W);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0 /*left*/, WIN_W * aspect /* right */, WIN_H /* bottom */,
          0 /* top */, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

void on_key(GLFWwindow * /*window*/, int key, int /*scancode*/, int action,
            int mods) {
  using namespace dfsparks;

  if (key == GLFW_KEY_V && action == GLFW_PRESS) {
    log_level =
        log_level == LogLevel::error ? LogLevel::debug : LogLevel::error;
  }
  if (key == GLFW_KEY_LEFT && action == GLFW_PRESS) {
    controlled.prev();
  }
  if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) {
    controlled.next();
  }
  if (key == GLFW_KEY_S && action == GLFW_PRESS) {
    controlled.serve();
  }
  if (key == GLFW_KEY_C && action == GLFW_PRESS) {
    controlled.listen();
  }
  if (key == GLFW_KEY_O && action == GLFW_PRESS) {
    controlled.disconnect();
  }
  if (key >= GLFW_KEY_0 && key < GLFW_KEY_9 && action == GLFW_PRESS) {
    controlled.play(key - GLFW_KEY_0 + (mods & GLFW_MOD_SHIFT ? 10 : 0));
  }
  if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
    controlled.knock();
  }
}

int main(int /*argc*/, char ** /*argv*/) {
  try {
    GLFWwindow *window;
    if (!glfwInit())
      return -1;

    window = glfwCreateWindow(WIN_W, WIN_H, WIN_TITLE, NULL, NULL);
    if (!window) {
      glfwTerminate();
      return -1;
    }
    glfwSetWindowSizeCallback(window, on_resize);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, on_key);

    on_resize(window, WIN_W, WIN_H);

    network.begin();
    for (auto &spark : sparks) {
      spark->begin();
    }
    controlled.serve();

    while (!glfwWindowShouldClose(window)) {
      glClear(GL_COLOR_BUFFER_BIT);

      for (auto &spark : sparks) {
        spark->render();
      }
      // render_circle(400, 300, 300, 0xff0000);

      std::stringstream title;
      title << WIN_TITLE << " | " << controlled.get_playing_effect().get_name();
      glfwSetWindowTitle(window, title.str().c_str());

      glfwSwapBuffers(window);
      glfwPollEvents();
    }

    for (auto &spark : sparks) {
      spark->end();
    }
    network.end();

    glfwTerminate();
    return 0;
  } catch (const std::exception &ex) {
    fprintf(stderr, "ERROR: %s\n", ex.what());
    return -1;
  }
}
