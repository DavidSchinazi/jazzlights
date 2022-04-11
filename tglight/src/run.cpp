#include <stdlib.h>
#include "tgloader.hpp"
#include "unisparks.hpp"

using namespace unisparks;

Player player;
Udp network;
const char* sysinfo();
char version[32];

extern "C" const char* call(const char* cmd) {
  if (!strcmp(cmd, "shutdown")) {
    if (!system("shutdown -h now")) {
      return "msg shutting down...";
    }
    return "! failed to shut down";
  }
  if (!strcmp(cmd, "sysinfo?")) {
    return sysinfo();
  }
  return player.command(cmd);
}

extern "C" void run(bool verbose, const char* ver, const char* cfgfile) {
  if (verbose) {
    enableVerboseOutput();    
  }

  strcpy(version, ver);
  info("My %s", sysinfo());
  load(cfgfile, player);
  player.connect(network);
  player.begin();
  for (;;) {
    player.render(timeMillis());
  }
  // runGui("TechnoGecko LED control", player, fullscreen);
}
