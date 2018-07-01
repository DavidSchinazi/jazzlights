#include <stdlib.h>
#include "loader.hpp"
#include "unisparks.hpp"

using namespace unisparks;

Player *player;

extern "C" const char* call(const char* cmd) {
  if (!strcmp(cmd, "shutdown")) {
    if (!system("shutdown -h now")) {
      return "msg shutting down...";
    }
    return "! failed to shut down";
  }
  return player->command(cmd);
}

extern "C" void run(const char* cfgfile) {
  player = &load(cfgfile);
  for (;;) {
    player->render();
  }
  // runGui("TechnoGecko LED control", player, fullscreen);
}
