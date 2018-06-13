#include <vector>
#include <sstream>
#include <iomanip>
#include <array>
#include <memory>
#include <GLFW/glfw3.h>
#include "unisparks.hpp"
#include "unisparks/renderers/opengl.hpp"
#include "unisparks/layouts/pixelmap.hpp"
#include "unisparks/util/geom3d.hpp"

using namespace std;
using namespace unisparks;

const char* WIN_TITLE = "Unisparks Demo";
const int WIN_W = 800;
const int WIN_H = 600;

Player player;
Udp network;
Matrix pixels(200, 150);
GLRenderer renderer(0, 0, WIN_W, WIN_H, pixels);

extern const Effect* OVERLAYS[];
extern const int OVERLAYS_CNT;

void onResize(GLFWwindow*, int width, int height) {
  double aspect = 1.0 * width / height;
  glViewport(0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0 /*left*/, WIN_W /* right */, WIN_W / aspect /* bottom */,
          0 /* top */, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

void onKey(GLFWwindow* window, int key, int /*scncode*/, int action,
           int mods) {
  if (key == GLFW_KEY_V && action == GLFW_PRESS) {
    // logLevel =
    //     (logLevel == errorLevel ? debugLevel : errorLevel);
  } else if (key == GLFW_KEY_LEFT && action == GLFW_PRESS) {
    player.prev();
  } else if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) {
    player.next();
  } else if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9 && action == GLFW_PRESS
             && !(mods & GLFW_MOD_SHIFT)) {
    player.play(key - GLFW_KEY_1);
  } else if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9 && action == GLFW_PRESS
             && (mods & GLFW_MOD_SHIFT)) {
    int i = (key - GLFW_KEY_1) % OVERLAYS_CNT;
    info("Playing overlay #%d", i);
    player.overlay(*OVERLAYS[i]);
  } else if (key == GLFW_KEY_S && action == GLFW_PRESS) {
    player.syncTest(0);
  } else if (key == GLFW_KEY_0 && action == GLFW_PRESS && (mods & GLFW_MOD_SHIFT)) {
    info("Clering overlay");
    player.clearOverlay();
  } else if (key == GLFW_KEY_N && action == GLFW_PRESS) {
    if (network.connected()) {
      network.disconnect();
    } else {
      network.reconnect();
    }
  }
  else if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
    player.paused() ? player.resume() : player.pause();
  } else if (key == GLFW_KEY_ESCAPE || (key == GLFW_KEY_C
                                        && (mods & GLFW_MOD_CONTROL))) {
    glfwSetWindowShouldClose(window, GL_TRUE);
  }
};



int main(int argn, char** argv) {
  bool fullscreen = false;

  // Init player
  player.begin(pixels, renderer, network);

  // Run GUI
  info("Running Unisparks demo");
  if (!glfwInit()) {
    return -1;
  }

  int winWidth = WIN_W;
  int winHeight = WIN_H;
  GLFWmonitor* monitor = nullptr;

  if (fullscreen) {
    monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    winWidth = mode->width;
    winHeight = mode->height;
  }

  GLFWwindow* window = glfwCreateWindow(winWidth, winHeight, WIN_TITLE,
                                        fullscreen ? glfwGetPrimaryMonitor() :
                                        nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    return -1;
  }

  glfwSetFramebufferSizeCallback(window, onResize);
  glfwMakeContextCurrent(window);
  glfwSetKeyCallback(window, onKey);

  int fbWidth, fbHeight;
  glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
  onResize(window, fbWidth, fbHeight);

  glClearColor(0.15, 0.15, 0.15, 1);
  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT);

    player.render();

    auto t = player.effectTime() / 1000;
    std::stringstream title;
    using std::setw;
    using std::setfill;
    title << WIN_TITLE <<
          " | " << player.effectName() <<
          " | " << (t / 60) << ":" << setw(2) << setfill('0') << (t % 60) <<
          " | " << (network.connected() ? network.isControllingEffects() ? "master" :
                    "slave" : "standalone");
    glfwSetWindowTitle(window, title.str().c_str());

    glfwSwapBuffers(window);
    glfwPollEvents();
  }
  glfwTerminate();

  return 0;
}
