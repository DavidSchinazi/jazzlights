#include <cstdio>
#include <thread>
#include <chrono>
#include "DFSparks.h"
#include "socketlib/server.h"
#include "demo/ui.h"

using std::printf;
using std::chrono::milliseconds;
using std::chrono::steady_clock;
using dfsparks::Server;
using dfsparks::rgb;
using dfsparks::hsl;

std::atomic<bool> stopped(false);

void blink(Server& server, uint32_t c1, uint32_t c2, int duration, int freq) {
  auto delay = milliseconds(1000/freq);
  auto t0 = steady_clock::now();
  do {
    server.broadcast_solid_color(c1);
    printf("c1\n");
    std::this_thread::sleep_for(delay);
    server.broadcast_solid_color(c2);
    printf("c2\n");
    std::this_thread::sleep_for(delay);
  } 
  while(!stopped && (steady_clock::now() - t0 < milliseconds(duration)));
}

void broadcast_frames() {

  try {
    Server server;
    while(!stopped) {
      blink(server, rgb(0,255,0), rgb(255,0,0), 5, 2);
    }

    // for(int n=0;;++n) {
    //   switch(n % 2) {
    //     case 0: 
    //       printf("GREEN\n");
    //       server.broadcast_solid_color(rgb(0,255,0)); 
    //       break; 

    //     case 1: 
    //       printf("RED\n");
    //       server.broadcast_solid_color(rgb(255,0,0)); 
    //       break; 
    //   }
    //   std::this_thread::sleep_for(milliseconds(1000));
    // }
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
    dfsparks::ui::run();
    stopped = true;
    server_thread.join();
  }
  catch(const std::exception& ex) {
    fprintf(stderr, "ERROR: %s\n", ex.what());
    return -1;
  }
  return 0;
}
