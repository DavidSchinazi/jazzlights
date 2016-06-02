#include "demo/ui.h"
#include <cstdio>
#include <exception>

using std::printf;

int main(int argc, char **argv) {
  try {
    dfsparks::ui::run();
  } catch (const std::exception &ex) {
    fprintf(stderr, "ERROR: %s\n", ex.what());
    return -1;
  }
  return 0;
}
