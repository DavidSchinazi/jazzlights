#include <stdlib.h>

#include "jazzlights/config.h"
#include "jazzlights/networks/unix_udp.h"
#include "jazzlights/player.h"
#include "sysinfo.h"
#include "tgloader.h"

namespace jazzlights {

Player player;
UnixUdpNetwork network;
char version[256] = {};

const char* callInner(const char* cmd) {
  if (!strcmp(cmd, "shutdown")) {
    if (!system("shutdown -h now")) { return "msg shutting down..."; }
    return "! failed to shut down";
  }
  if (!strcmp(cmd, "sysinfo?")) { return sysinfo(); }
  return player.command(cmd);
}

void runInner(bool verbose, const char* ver, const char* cfgfile) {
  if (verbose) { enable_debug_logging(); }

  snprintf(version, sizeof(version) - 1, "%s_%s", ver, BOOT_MESSAGE);
  info("My %s", sysinfo());
  player.setBasePrecedence(20000);
  player.setPrecedenceGain(5000);
  load(cfgfile, player);
  player.connect(&network);
  player.begin(timeMillis());
  while (true) { player.render(timeMillis()); }
  // runGui("TechnoGecko LED control", player, fullscreen);
}

}  // namespace jazzlights

extern "C" const char* call(const char* cmd) { return jazzlights::callInner(cmd); }

extern "C" void run(bool verbose, const char* ver, const char* cfgfile) {
  return jazzlights::runInner(verbose, ver, cfgfile);
}
