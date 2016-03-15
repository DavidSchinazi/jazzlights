#include <cstdio>
#include <thread>
#include <chrono>
#include "server/server.h"
#include "arduinolib/DiscoFish_Color.h"

void broadcast_frames() {
  using std::printf;
  using dfwearables::Server;

  try {
    Server server;

    for(int n=0;;++n) {
      switch(n % 2) {
        case 0: 
          printf("GREEN\n");
          server.broadcast_solid_color(DFWP_RGB(0,255,0)); 
          break; 

        case 1: 
          printf("RED\n");
          server.broadcast_solid_color(DFWP_RGB(255,0,0)); 
          break; 
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
  catch(const std::exception& ex) {
    fprintf(stderr, "ERROR: %s\n", ex.what());
  }  
}

int main(int argc, char **argv) {
  using std::printf;

  try {
    std::thread server_thread(broadcast_frames);
    server_thread.join();
  }
  catch(const std::exception& ex) {
    fprintf(stderr, "ERROR: %s\n", ex.what());
    return -1;
  }
  return 0;
}
