#ifndef DFWP_CLIENT_H
#define DFWP_CLIENT_H
#include <functional>
#include "dfwp/protocol.h"

namespace dfwp {
   class Client {
   public:
      typedef std::function<void()> random_frame_handler_t;
      typedef std::function<void(uint32_t)> solid_frame_handler_t;

      Client(int port=DFWP_PORT);
      Client(const Server&) = delete;
      ~Client();

      Client& operator=(Client&) = delete;

      void set_solid_handler(solid_frame_handler_t cb) { solid_frame_handler = cb; } 

   private:
      solid_frame_handler_t solid_frame_handler;
      random_frame_handler_t random_frame_handler;
   };
}

#endif /* DFWP_CLIENT_H */