#include "gui.h"

#include <GLFW/glfw3.h>

#include <array>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

#include "glrenderer.h"
#include "jazzlights/config.h"
#include "jazzlights/player.h"

namespace jazzlights {

const int WIN_W = 960;
const int WIN_H = 720;

static Player* player = nullptr;
static Box viewport = {
    {0.0, 0.0},
    {0.0, 0.0}
};

void onResize(GLFWwindow*, int winWidth, int winHeight) {
  const Box& vp = viewport;
  double aspect = winHeight * width(vp) / (winWidth * height(vp));

  glViewport(0, 0, winWidth, winHeight);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(left(vp), right(vp), top(vp) + height(vp) * aspect, top(vp), -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

void onKey(GLFWwindow* window, int key, int /*scncode*/, int action, int mods) {
  const Milliseconds currentTime = timeMillis();
  if (key == GLFW_KEY_LEFT && action == GLFW_PRESS) {
    player->loopOne(currentTime);
  } else if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) {
    player->stopLooping(currentTime);
    player->next(currentTime);
  } else if (key == GLFW_KEY_0 && action == GLFW_PRESS && (mods & GLFW_MOD_SHIFT)) {
  } else if (key == GLFW_KEY_ESCAPE || (key == GLFW_KEY_C && (mods & GLFW_MOD_CONTROL))) {
    glfwSetWindowShouldClose(window, GL_TRUE);
  }
};

int runGui(const char* winTitle, Player& playerRef, Box vp, bool fullscreen, Milliseconds killTime) {
  player = &playerRef;
  viewport = vp;

  jll_info("Running GUI, view box is (%0.3f, %0.3f) - (%0.3f, %0.3f) meters, using GLFW v.%s", left(player->bounds()),
           top(player->bounds()), right(player->bounds()), bottom(player->bounds()), glfwGetVersionString());

  if (!glfwInit()) { jll_fatal("Can't initialize graphics"); }

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

  GLFWwindow* window =
      glfwCreateWindow(winWidth, winHeight, winTitle, fullscreen ? glfwGetPrimaryMonitor() : nullptr, nullptr);
  if (!window) { jll_fatal("Can't create window"); }

  glfwSetFramebufferSizeCallback(window, onResize);
  glfwMakeContextCurrent(window);
  glfwSetKeyCallback(window, onKey);

  int fbWidth, fbHeight;
  glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
  onResize(window, fbWidth, fbHeight);

  glClearColor(0.15, 0.15, 0.15, 1);
  while (!glfwWindowShouldClose(window)) {
    Milliseconds currentTime = timeMillis();
    if (killTime > 0 && currentTime > killTime) {
      jll_info("Kill time reached, exiting.");
      exit(0);
    }
    glClear(GL_COLOR_BUFFER_BIT);

    player->render(currentTime);
    std::ostringstream title;
    title << "jazzlights-demo-" << REVISION << " " << player->currentEffectName();
    glfwSetWindowTitle(window, title.str().c_str());

    glfwSwapBuffers(window);
    glfwPollEvents();
  }
  glfwTerminate();
  return 0;
}

}  // namespace jazzlights
