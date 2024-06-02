#ifndef JL_REST_SERVER_H
#define JL_REST_SERVER_H

#include "jazzlights/config.h"

#ifndef JL_REST_SERVER
#if JL_IS_CONFIG(CLOUDS)
#define JL_REST_SERVER 1
#else  // CLOUDS
#define JL_REST_SERVER 0
#endif  // CLOUDS
#endif  // JL_REST_SERVER

#if JL_REST_SERVER

#include <ESPAsyncWebServer.h>

#include "jazzlights/player.h"

namespace jazzlights {

// RestServer is work in progress, it is not yet ready for use.

class RestServer : public Player::StatusWatcher {
 public:
  void Start();

  explicit RestServer(uint16_t port, Player& player);

  // From Player::StatusWatcher.
  void OnStatus() override;

  void PauseUpdates();
  void ResumeUpdates();

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
  // Prevent the RestServer from sending any updates while this object is in scope. This avoids sending
  // repeated updates while changing multiple fields in response to a message. If any updates are attempted while the
  // pause is in effect, an update will be sent when this goes out of scope.
  class ScopedUpdatePauser {
   public:
    explicit ScopedUpdatePauser(RestServer* server) : server_(server) { server_->PauseUpdates(); }
    ~ScopedUpdatePauser() { server_->ResumeUpdates(); }

   private:
    RestServer* server_;
  };
  AsyncWebServer server_;
  WebSocket web_socket_;
  Player& player_;
  bool started_ = false;
  enum class PausedUpdateState {
    kOpen = 0,
    kPausedNoUpdate = 1,
    kPausedUpdateOneClient = 2,
    kPausedUpdateAllClients = 3,
  };
  PausedUpdateState paused_update_state_ = PausedUpdateState::kOpen;
  AsyncWebSocketClient* client_to_update_ = nullptr;
};

}  // namespace jazzlights

#endif  // JL_REST_SERVER
#endif  // JL_REST_SERVER_H
