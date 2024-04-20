#ifndef JL_REST_SERVER_H
#define JL_REST_SERVER_H

#include "jazzlights/config.h"

#if JL_IS_CONFIG(CLOUDS)

#include <ESPAsyncWebServer.h>

#include "jazzlights/player.h"

namespace jazzlights {

// RestServer is work in progress, it is not yet ready for use.

class RestServer {
 public:
  void Start();

  explicit RestServer(uint16_t port, Player& player);

 private:
  AsyncWebServer server_;
  AsyncWebSocket web_socket_;
  Player& player_;
  bool started_ = false;
};

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(CLOUDS)
#endif  // JL_REST_SERVER_H
