#include "unisparks.hpp"
#include "loader.hpp"
#include "gui.hpp"
using namespace unisparks;

int main(int argn, char** argv) {
  if (argn<2) {
    printf("Usage: %s <config>\n", argv[0]);
    exit(-1);
  }
  bool fullscreen = false;
  const char* config = argv[1];

  Box viewport;
  Player player;
  Udp network;
  player.connect(network);
  load(config, player, viewport);

  return runGui("Unisparks Demo", player, viewport, fullscreen);
}
