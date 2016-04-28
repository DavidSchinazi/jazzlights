#include "DFSparks.h"
#include "demo/client.h"
#include "demo/server.h"
#include "demo/task.h"
#include "demo/ui.h"
#include <cstdio>

using std::printf;
using dfsparks::Task;

int main(int argc, char **argv) {
  try {
    const char *mode = argc > 1 ? argv[1] : "client+server";

    // printf("DiscoFish Sparks %s (%s/%d.%d), port %d)\n",
    //   mode, dfsparks::proto_name, dfsparks::proto_version_major,
    //   dfsparks::proto_version_minor, dfsparks::udp_port);

    // Task server(dfsparks::server::run);
    // Task client(dfsparks::client::run);

    // if (!strcmp(mode, "server")) {
    //  server.run();
    //}
    // else if (!strcmp(mode, "client")) {
    //  client.start();
    //   dfsparks::ui::run();
    //   printf("client is shutting down\n");
    // } else {
    //   server.start();
    //   client.start();
    //   dfsparks::ui::run();
    // }
    dfsparks::ui::run();

  } catch (const std::exception &ex) {
    fprintf(stderr, "ERROR: %s\n", ex.what());
    return -1;
  }
  return 0;
}
