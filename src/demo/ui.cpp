#include "DFSparks.h"
#include "demo/socket.h"
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


class SocketClient : public Client {
private:
  int on_begin(int udp_port) override {
    sock.set_reuseaddr(true);
    sock.set_nonblocking(true);
    sock.bind("*", udp_port);
    return 0;
  }

  int on_recv(char *buf, size_t bufsize) override {
     return sock.recvfrom(buf, bufsize);
  }

  Socket sock;
};

class SocketServer : public Server {
private:
  int on_begin() override {
    sock.set_nonblocking(true);
    sock.set_broadcast(true);
    return 0;
  }

  int on_send(const char *addr, int port, char *buf, size_t bufsize) override {
    sock.sendto("255.255.255.255", port, buf, bufsize);
    return bufsize;
  }

  Socket sock;
};

class Vest : public Matrix<17,8> {
public:

  Vest(double x, double y) : x0(x), y0(y) {
  }
  ~Vest() {}


private:

  void on_set_pixel_color(int x, int y, uint8_t red, uint8_t green,
                        uint8_t blue) {
    render_led(x0 + 2.5 * LED_R * x, y0 + 2.5 * LED_R * y,
               rgb(red, green, blue));
  }

  double x0;
  double y0;
};

template<int length>
class Ring : public Matrix<length,1> {
public:
  Ring(double cx, double cy)
      : r(LED_DIST * length / TWO_PI), cx(cx), cy(cy) {
      }
  ~Ring() {}

private:

  void on_set_pixel_color(int i, int y, uint8_t red, uint8_t green,
                        uint8_t blue) override {
    render_led(cx + (r * cos(i * TWO_PI / length)),
               cy + (r * sin(i * TWO_PI / length)), rgb(red, green, blue));
  };

  // void set_status_led_(bool ison) override {
  //   render_led(cx, cy, ison ? rgb(255,255,255) : rgb(50,50,50));
  // };

  double r;
  double cx;
  double cy;
};

// class Ring : public Matrix {
// public:
//   Ring(double cx, double cy, int length = 12)
//       : length(length), r(LED_DIST * length / TWO_PI), cx(cx), cy(cy) {

//         size = length; ///3.14 + 1;
//         mask = new int[ size * size ];
//         for(int i=0; i<length; ++i) {
//           int x = size/2 + size/2 * cos(i * TWO_PI / length);
//           int y = size/2 + size/2 * sin(i * TWO_PI / length);
//           printf(">>> sz:%d, x:%d, y:%d\n", size, x, y);
//           mask[y*size + x] = i+1;
//         }
//       }

//   ~Ring2() {
//     delete[] mask;
//   }

// private:
//   int width_() const override { return size; }
//   int height_() const override { return size; }

//   void set_pixel_(int x, int y, uint8_t red, uint8_t green,
//                         uint8_t blue) override {
//     // render_led(cx + 2.5 * LED_R * x, cy + 2.5 * LED_R * y,
//     //            rgb(red, green, blue));
//     int i = mask[x + y*size];
//     if (i) {
//       i = i - 1;
//       render_led(cx + (r * cos(i * TWO_PI / length)),
//                  cy + (r * sin(i * TWO_PI / length)), rgb(red, green, blue));
//     }
//   };

//   void set_status_led_(bool ison) override {
//     render_led(cx, cy, ison ? rgb(255,255,255) : rgb(50,50,50));
//   };

//   int recv_packet_(char *buf, size_t bufsize) override {
//     return sock.recvfrom(buf, bufsize);
//   }

//   int send_packet_(char *buf, size_t bufsize) override {
//     sock.sendto("255.255.255.255", udp_port, buf, bufsize);
//     return bufsize;
//   }

//   int32_t time_ms() const override { return millis(); }


//   int size;
//   int *mask;
//   int length;
//   double r;
//   double cx;
//   double cy;
// };

SocketClient cli;
SocketServer srv;

Vest vest1(-0.5, -0.5);
Ring<12> ring1(0, 0.5);

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
    vest1.play_prev();
  }
  if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) {
    vest1.play_next();
  }
  if (key == GLFW_KEY_S && action == GLFW_PRESS) {
    //vest1.set_network_mode(Matrix::SERVER);
  }
  if (key == GLFW_KEY_C && action == GLFW_PRESS) {
    //vest1.set_network_mode(Matrix::CLIENT);
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

  vest1.begin(srv);
  ring1.begin(cli);

  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT);

    vest1.render();
    ring1.render();
    printf(">>> start time: %d\n", cli.get_start_time());

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwTerminate();
  return 0;
}

} // namespace ui
} // namespace dfsparks
