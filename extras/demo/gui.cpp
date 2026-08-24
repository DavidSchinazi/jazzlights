#include "gui.h"

#include <GLFW/glfw3.h>

#include <array>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

#include "glrenderer.h"
#include "jazzlights/render/player.h"
#include "jazzlights/util/config.h"

namespace jazzlights {

const int kWinWidth = 960;
const int kWinHeight = 720;

static Player* sPlayer = nullptr;
static Box sViewport = {
    {0.0, 0.0},
    {0.0, 0.0}
};

void OnResize(GLFWwindow*, int winWidth, int winHeight) {
  const Box& vp = sViewport;
  double aspect = winHeight * Width(vp) / (winWidth * Height(vp));

  glViewport(0, 0, winWidth, winHeight);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(Left(vp), Right(vp), Top(vp) + Height(vp) * aspect, Top(vp), -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

void OnKey(GLFWwindow* window, int key, int /*scncode*/, int action, int mods) {
  if (key == GLFW_KEY_LEFT && action == GLFW_PRESS) {
    sPlayer->LoopOne();
  } else if (key == GLFW_KEY_RIGHT && action == GLFW_PRESS) {
    sPlayer->StopLooping();
    sPlayer->Next();
  } else if (key == GLFW_KEY_0 && action == GLFW_PRESS && (mods & GLFW_MOD_SHIFT)) {
  } else if (key == GLFW_KEY_ESCAPE || (key == GLFW_KEY_C && (mods & GLFW_MOD_CONTROL))) {
    glfwSetWindowShouldClose(window, GL_TRUE);
  }
};

int RunGui(const char* winTitle, Player& playerRef, Box vp, bool fullscreen, OptionalMicroseconds killTime) {
  sPlayer = &playerRef;
  sViewport = vp;

  jll_info("Running GUI, view box is (%0.3f, %0.3f) - (%0.3f, %0.3f) meters, using GLFW v.%s", Left(sPlayer->bounds()),
           Top(sPlayer->bounds()), Right(sPlayer->bounds()), Bottom(sPlayer->bounds()), glfwGetVersionString());

  if (!glfwInit()) { jll_fatal("Can't initialize graphics"); }

  int winWidth = kWinWidth;
  int winHeight = kWinHeight;
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

  glfwSetFramebufferSizeCallback(window, OnResize);
  glfwMakeContextCurrent(window);
  glfwSetKeyCallback(window, OnKey);

  int fbWidth, fbHeight;
  glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
  OnResize(window, fbWidth, fbHeight);

  glClearColor(0.15, 0.15, 0.15, 1);
  while (!glfwWindowShouldClose(window)) {
    if (killTime && TimeMicros() > *killTime) {
      jll_info("Kill time reached, exiting.");
      exit(0);
    }
    glClear(GL_COLOR_BUFFER_BIT);

    sPlayer->Render();
    std::ostringstream title;
    title << "jazzlights-demo-" << REVISION << " " << sPlayer->CurrentEffectName();
    glfwSetWindowTitle(window, title.str().c_str());

    glfwSwapBuffers(window);
    glfwPollEvents();
  }
  glfwTerminate();
  return 0;
}

}  // namespace jazzlights
