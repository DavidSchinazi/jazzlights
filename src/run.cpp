#include <stdlib.h>
#include "tgloader.hpp"
#include "unisparks.hpp"

using namespace unisparks;

Player player;
Udp network;


extern "C" const char* call(const char* cmd) {
  if (!strcmp(cmd, "shutdown")) {
    if (!system("shutdown -h now")) {
      return "msg shutting down...";
    }
    return "! failed to shut down";
  }
  return player.command(cmd);
}

extern "C" void run(bool verbose, const char* cfgfile) {
  if (verbose) {
    enableVerboseOutput();    
  }

  load(cfgfile, player);
  player.connect(network);
  player.begin();
  for (;;) {
    player.render();
  }
  // runGui("TechnoGecko LED control", player, fullscreen);
}
