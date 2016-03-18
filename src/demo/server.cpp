#include <thread>
#include <chrono>
#include <functional>

#include "demo/server.h"
#include "demo/socket.h"
#include "DFSparks.h"

using std::chrono::milliseconds;
using std::chrono::steady_clock;
using std::chrono::duration_cast;
using dfsparks::rgb;
using dfsparks::hsl;

namespace dfsparks {
  namespace server {

  template<typename Encoder, typename... Args> 
  void broadcast(Socket& sock, Encoder& encoder, Args... args) {
      char buf[frame::max_size];
      size_t size = encoder(buf, sizeof(buf), args...);
      if (size == 0) {
        throw std::runtime_error("Can't encode frame");
      }
      sock.sendto("255.255.255.255", udp_port, buf, size);
      //ui::update(buf, size);
  }


    struct Context {
      uint64_t elapsed;
    };

    size_t none(const Context& context, void *buf, size_t bufsize) {
      return 0;
    }

    size_t blink(const Context& context, void *buf, size_t bufsize) {
      uint32_t color = context.elapsed % 1000 > 500 ? rgb(255,0,0) : rgb(0,255,0);
      return frame::solid::encode(buf, bufsize, color);
    }

    size_t chameleon(const Context& context, void *buf, size_t bufsize) {
      const int duration = 3000;
      double hue = 1.0*(context.elapsed % duration)/duration;
      return frame::solid::encode(buf, bufsize, hsl(hue, 1, 1));
    }

    size_t random(const Context& context, void *buf, size_t bufsize) {
      const int duration = 100;
      return frame::random::encode(buf, bufsize, context.elapsed/duration);
    }

    size_t rainbow(const Context& context, void *buf, size_t bufsize) {
      const int duration = 1000;
      double from_hue = 1.0*(context.elapsed % duration)/duration;
      double to_hue = from_hue + 1;
      printf("TX: hue %f:%f\n", from_hue, to_hue);
      return frame::rainbow::encode(buf, bufsize, from_hue, to_hue, 1, 1);
    }

    std::function<size_t(const Context&, void*, size_t)> 
    cycle_effects(uint64_t elapsed) {
      switch(elapsed/5000 % 4) {
        case 0: return rainbow;
        case 1: return blink;
        case 2: return chameleon;
        case 3: return random;
        default: return none;
      }
    }

    void run(std::atomic<bool>& stopped) {
      printf("server started\n");
      Socket sock;
      sock.set_reuseaddr(true);
      sock.set_broadcast(true);

      Context ctx {};
      char buf[frame::max_size];
      auto t0 = steady_clock::now();

      while(!stopped) {
        ctx.elapsed = duration_cast<milliseconds>(steady_clock::now() - t0).count();
        size_t size = cycle_effects(ctx.elapsed)(ctx, buf, sizeof(buf));   
        if (size) {
          sock.sendto("255.255.255.255", udp_port, buf, size);
        }
        std::this_thread::sleep_for(milliseconds(100));
      } 

      printf("server ended\n");
    }

  } // namespace server
} // namespace dfsparks
