#include "loader.hpp"
#include "gui.hpp"

int main(int argn, char** argv) {
  if (argn<2) {
    printf("Usage: %s <config>\n", argv[0]);
    exit(-1);
  }
  bool fullscreen = false;
  
  return runGui("Unisparks Demo", load(argv[1]), fullscreen);
}
