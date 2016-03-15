#include <cstdio>
#include <thread>
#include <chrono>
#include "DFSparks.h"
#include "server/server.h"

void broadcast_frames() {
  using std::printf;
  using std::chrono::milliseconds;
  using dfsparks::Server;
  using dfsparks::rgb;

  try {
    Server server;

    for(int n=0;;++n) {
      switch(n % 2) {
        case 0: 
          printf("GREEN\n");
          server.broadcast_solid_color(rgb(0,255,0)); 
          break; 

        case 1: 
          printf("RED\n");
          server.broadcast_solid_color(rgb(255,0,0)); 
          break; 
      }
      std::this_thread::sleep_for(milliseconds(1000));
    }
  }
  catch(const std::exception& ex) {
    fprintf(stderr, "ERROR: %s\n", ex.what());
  }  
}

int main(int argc, char **argv) {
  using std::printf;

  try {
    printf("DiscoFish sparks server (DFSP/" VERSION ", port %d)\n", 
      dfsparks::udp_port);
    std::thread server_thread(broadcast_frames);
    server_thread.join();
  }
  catch(const std::exception& ex) {
    fprintf(stderr, "ERROR: %s\n", ex.what());
    return -1;
  }
  return 0;
}
