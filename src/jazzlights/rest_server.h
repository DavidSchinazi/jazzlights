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
  void HandleMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len);
  void ShareStatus(AsyncWebSocketClient* client);
  class WebSocket : public AsyncWebSocket {
   public:
    explicit WebSocket(const String& url, RestServer* rest_server) : AsyncWebSocket(url), rest_server_(rest_server) {}

    static void EventHandler(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg,
                             uint8_t* data, size_t len);

   private:
    RestServer* rest_server_;  // Unowned.
  };
  AsyncWebServer server_;
  WebSocket web_socket_;
  Player& player_;
  bool started_ = false;
};

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(CLOUDS)
#endif  // JL_REST_SERVER_H
