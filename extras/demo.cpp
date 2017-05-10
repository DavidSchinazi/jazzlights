#include "dfsparks/log.h"
#include "dfsparks/player.h"
#include "dfsparks/math.h"
#include "dfsparks/networks/udpsocket.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdio.h>
#include <vector>
#include <functional>

const char *WIN_TITLE = "DFSparks Demo";
const int WIN_W = 800;
const int WIN_H = 600;

const double TWO_PI = 2 * 3.1415926;

void renderCircle(double cx, double cy, double r, dfsparks::RgbaColor color,
                   int segments = 25) {
  using namespace dfsparks;

  glColor4f(color.red / 255.f, color.green / 255.f, color.blue / 255.f, color.alpha / 255.f);
  glBegin(GL_TRIANGLE_FAN);
  glVertex2f(cx, cy);
  for (int i = 0; i <= segments; i++) {
    glVertex2f(cx + (r * cos(i * TWO_PI / segments)),
               cy + (r * sin(i * TWO_PI / segments)));
  }
  glEnd();
}

dfsparks::UdpSocketNetwork network;

class Matrix : public dfsparks::PixelMatrix {
public:
  Matrix(int w, int h, double vpl, double vpt, double vpw, double vph)
      : dfsparks::PixelMatrix(w, h, setColor, this), 
        led_sp(fmin(vpw/w, vph/h)),
        viewport_left(vpl), 
        viewport_top(vpt) {}

private:
  static void setColor(int i, dfsparks::RgbaColor color, void *m) {
    static_cast<Matrix*>(m)->setColor(i, color);
  }

  void setColor(int i, dfsparks::RgbaColor color)  {
    dfsparks::Point p = coords(i);
    renderCircle(viewport_left + led_sp * (p.x + 0.5),
                  viewport_top + led_sp * (p.y + 0.5), 0.4 * led_sp, color);  
  }

  double led_sp;
  double viewport_left;
  double viewport_top;
};


class Ring : public dfsparks::PixelMatrix {
public:
    Ring(int length, double vpl, double vpt, double vpw, double vph)
        : dfsparks::PixelMatrix(1, length, setColor, this), cx(vpl + vpw / 2), cy(vpt + vph / 2),
          r(0.8 * fmin(vpw, vph) / 2), led_sp(0.5 * TWO_PI * r / length) {}

private:
  static void setColor(int i, dfsparks::RgbaColor color, void *m) {
    static_cast<Ring*>(m)->setColor(i, color);
  }

  void setColor(int i, dfsparks::RgbaColor color)  {
      int length = height();
      double x = r * cos(i * TWO_PI / length);
      double y = r * sin(i * TWO_PI / length);
      renderCircle(cx + x, cy + y, 0.8 * led_sp, color);
  }

    double cx;
    double cy;
    double r;
    double led_sp;
};


std::vector<std::function<dfsparks::NetworkPlayer&(void)>> sparks = {
    []() -> dfsparks::NetworkPlayer& {
        static Matrix pixels(250, 50, 0.0, 0.0, 800.0, 160.0);
        static dfsparks::NetworkPlayer player(pixels, network);
        return player;
    },
    []() -> dfsparks::NetworkPlayer& {
        static Matrix pixels(8, 17, 40.0, 170.0, 160.0, 440.0);
        static dfsparks::NetworkPlayer player(pixels, network);
        return player;
    },
    []() -> dfsparks::NetworkPlayer& {
        static Ring pixels(12, 250.0, 200.0, 90.0, 90.0);
        static dfsparks::NetworkPlayer player(pixels, network);
        return player;
    },
  };

auto &controlled = sparks[0]();

void onResize(GLFWwindow * /*window*/, int width, int height) {
  double aspect = 1.0 * width * WIN_H / (height * WIN_W);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0 /*left*/, WIN_W * aspect /* right */, WIN_H /* bottom */,
          0 /* top */, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

void onKey(GLFWwindow * /*window*/, int key, int /*scancode*/, int action,
            int mods) {
  using namespace dfsparks;

  if (key == GLFW_KEY_V && action == GLFW_PRESS) {
    logLevel =
        (logLevel == errorLevel ? debugLevel : errorLevel);
  }
  else if (key == GLFW_KEY_LEFT && action == GLFW_PRESS) {
    controlled.prev();
  }
  else if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) {
    controlled.next();
  }
  else if (key == GLFW_KEY_M && action == GLFW_PRESS) {
    if (controlled.isMaster()) { 
        controlled.setSlave();
    } else {
      controlled.setMaster();
    }
  }
  else if (key == GLFW_KEY_D && action == GLFW_PRESS) {
    if (network.isConnected()) {
      network.disconnect();
    }
    else {
      network.reconnect();
    }
  }
  else if (key >= GLFW_KEY_0 && key < GLFW_KEY_9 && action == GLFW_PRESS) {
    controlled.play(key - GLFW_KEY_0 + (mods & GLFW_MOD_SHIFT ? 10 : 0));
  }
  else if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
    controlled.knock();
  }
  else if (key == GLFW_KEY_S && action == GLFW_PRESS) {
    controlled.showStatus();
  }
}

int main(int argc, char ** argv) {
  try {
    if (argc > 1 && !strcmp(argv[1],"-v")) {
      dfsparks::logLevel = dfsparks::debugLevel;
    }

    GLFWwindow *window;
    if (!glfwInit())
      return -1;

    window = glfwCreateWindow(WIN_W, WIN_H, WIN_TITLE, NULL, NULL);
    if (!window) {
      glfwTerminate();
      return -1;
    }
    glfwSetWindowSizeCallback(window, onResize);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, onKey);

    onResize(window, WIN_W, WIN_H);

    controlled.setMaster();
    controlled.shuffleAll();

    while (!glfwWindowShouldClose(window)) {
      glClear(GL_COLOR_BUFFER_BIT);

      for (auto &spark : sparks) {
          spark.operator()().render();
      }
      network.poll();
 
      std::stringstream title;
      title << WIN_TITLE << " | " << controlled.effectName();
      glfwSetWindowTitle(window, title.str().c_str());

      glfwSwapBuffers(window);
      glfwPollEvents();
    }

    glfwTerminate();
    return 0;
  } catch (const std::exception &ex) {
    fprintf(stderr, "ERROR: %s\n", ex.what());
    return -1;
  }
}
