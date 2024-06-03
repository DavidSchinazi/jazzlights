#include "jazzlights/rest_server.h"

#if JL_REST_SERVER

#include <ESPAsyncWebServer.h>
#include <mdns.h>

#include "jazzlights/util/log.h"

namespace jazzlights {

namespace {
static void sNotFoundHandler(AsyncWebServerRequest* request) {
  jll_info("RestServer got unexpected request for \"%s\"", request->url().c_str());
  request->send(404, "text/plain", String("Not found nope for ") + request->url());
}
}  // namespace

enum class WSType : uint8_t {
  kInvalid = 0,
  kStatusRequest = 1,
  kStatusShare = 2,
  kStatusSetOn = 3,
  kStatusSetBrightness = 4,
  kStatusSetColor = 5,
  kStatusSetEffect = 6,
};

enum WSStatusFlag : uint8_t {
  kWSStatusFlagOn = 0x80,
  kWSStatusFlagColorOverride = 0x40,
};

// static
void RestServer::WebSocket::EventHandler(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type,
                                         void* arg, uint8_t* data, size_t len) {
  WebSocket* web_socket = static_cast<WebSocket*>(server);
  switch (type) {
    case WS_EVT_CONNECT:
      jll_info("WebSocket client #%u connected from %s:%u", client->id(), client->remoteIP().toString().c_str(),
               client->remotePort());
      break;
    case WS_EVT_DISCONNECT: jll_info("WebSocket client #%u disconnected", client->id()); break;
    case WS_EVT_DATA: {
      AwsFrameInfo* info = reinterpret_cast<AwsFrameInfo*>(arg);
      if (info->final && info->index == 0 && info->len == len) {
        jll_info("WebSocket received unfragmented %s message of length %llu from client #%u ",
                 ((info->opcode == WS_TEXT) ? "text" : "binary"), info->len, client->id());
        web_socket->rest_server_->HandleMessage(client, data, len);
      } else {
        jll_info("WebSocket received fragmented %s message at index %llu of length %llu with%s FIN from client #%u",
                 ((info->opcode == WS_TEXT) ? "text" : "binary"), info->index, info->len, (info->final ? "" : "out"),
                 client->id());
        // TODO buffer and handle fragmented messages.
      }
    } break;
    case WS_EVT_PONG:
      jll_info("WebSocket received pong of length %u: %s from  client #%u", len,
               (len ? reinterpret_cast<char*>(data) : "<empty>"), client->id());
      break;
    case WS_EVT_ERROR:
      jll_info("WebSocket received error[%u]: %s from client #%u", *reinterpret_cast<uint16_t*>(arg),
               reinterpret_cast<char*>(data), client->id());
      break;
  }
}

void RestServer::HandleMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
  jll_info("Handling message of length %zu first byte %u from client #%u", len, (len > 0 ? data[0] : 0), client->id());
  ScopedUpdatePauser pauser(this);
  if (len == 0) { return; }
  switch (static_cast<WSType>(data[0])) {
    case WSType::kStatusRequest: {
      jll_info("Got status request from client #%u", client->id());
      ShareStatus(client);
    } break;
    case WSType::kStatusSetOn: {
      if (len < 2) {
        jll_error("Ignoring unexpectedly short set on status request of length %zu", len);
        break;
      }
      bool enabled = (data[1] & kWSStatusFlagOn) != 0;
      jll_info("Got turn %s request from client #%u", (enabled ? "on" : "off"), client->id());
      player_.set_enabled(enabled);
      if (!enabled) {
        // Reset to default parameters when turned off.
        player_.set_brightness(255);
        player_.disable_color_override();
      }
      ShareStatus(client);
    } break;
    case WSType::kStatusSetBrightness: {
      if (len < 2) {
        jll_error("Ignoring unexpectedly short set brightness request of length %zu", len);
        break;
      }
      uint8_t brightness = data[1];
      jll_info("Got brightness=%u request from client #%u", brightness, client->id());
      player_.set_enabled(true);
      player_.set_brightness(brightness);
      ShareStatus(client);
    } break;
    case WSType::kStatusSetColor: {
      if (len < 4) {
        jll_error("Ignoring unexpectedly short set color request of length %zu", len);
        break;
      }
      uint8_t r = data[1];
      uint8_t g = data[2];
      uint8_t b = data[3];
      jll_info("Got color=%02x%02x%02x request from client #%u", r, g, b, client->id());
      player_.set_enabled(true);
      player_.enable_color_override(CRGB(r, g, b));
      ShareStatus(client);
    } break;
    case WSType::kStatusSetEffect: {
      jll_info("Got effect request from client #%u", client->id());
      player_.set_enabled(true);
      player_.disable_color_override();
      ShareStatus(client);
    } break;
  }
}

void RestServer::PauseUpdates() {
  if (paused_update_state_ != PausedUpdateState::kOpen) { jll_fatal("Tried to double pause rest server updates"); }
  paused_update_state_ = PausedUpdateState::kPausedNoUpdate;
  jll_debug("Pausing rest server updates");
}

void RestServer::ResumeUpdates() {
  PausedUpdateState previous_state = paused_update_state_;
  paused_update_state_ = PausedUpdateState::kOpen;
  switch (previous_state) {
    case PausedUpdateState::kOpen: jll_fatal("Tried to resume non-paused rest server updates"); break;
    case PausedUpdateState::kPausedNoUpdate: jll_debug("Resuming rest server updates with none pending"); break;
    case PausedUpdateState::kPausedUpdateOneClient:
      jll_debug("Resuming rest server updates with pending to client #%u", client_to_update_->id());
      ShareStatus(client_to_update_);
      break;
    case PausedUpdateState::kPausedUpdateAllClients:
      jll_debug("Resuming rest server updates with pending to all clients");
      ShareStatus(nullptr);
      break;
    default: jll_fatal("Tried to resume rest server updates from unexpected state %d", paused_update_state_); break;
  }
  client_to_update_ = nullptr;
}

void RestServer::ShareStatus(AsyncWebSocketClient* client) {
  if (paused_update_state_ != PausedUpdateState::kOpen) {
    if (client == nullptr) {
      paused_update_state_ != PausedUpdateState::kPausedUpdateAllClients;
      client_to_update_ = nullptr;
    } else if (paused_update_state_ == PausedUpdateState::kPausedNoUpdate) {
      paused_update_state_ = PausedUpdateState::kPausedUpdateOneClient;
      client_to_update_ = client;
    } else if (paused_update_state_ == PausedUpdateState::kPausedUpdateOneClient && client_to_update_ != client) {
      paused_update_state_ != PausedUpdateState::kPausedUpdateAllClients;
      client_to_update_ = nullptr;
    }
    jll_debug("Skipping update to client #%u because we are paused", (client ? client->id() : 0));
    return;
  }
  if (player_.color_overridden()) {
    jll_info("Sending status %s brightness=%u color=%02x%02x%02x to client #%u", (player_.enabled() ? "on" : "off"),
             player_.brightness(), player_.color_override().r, player_.color_override().g,

             player_.color_override().b, (client ? client->id() : 0));
  } else {
    jll_info("Sending status %s brightness=%u to client #%u", (player_.enabled() ? "on" : "off"), player_.brightness(),
             (client ? client->id() : 0));
  }
  uint8_t response[6] = {};
  size_t response_length = 3;
  response[0] = static_cast<uint8_t>(WSType::kStatusShare);
  if (player_.enabled()) { response[1] |= kWSStatusFlagOn; }
  response[2] = player_.brightness();
  if (player_.color_overridden()) {
    response[1] |= kWSStatusFlagColorOverride;
    response[3] = player_.color_override().r;
    response[4] = player_.color_override().g;
    response[5] = player_.color_override().b;
    response_length += 3;
  }
  if (client != nullptr) {
    client->binary(&response[0], response_length);
  } else {
    web_socket_.binaryAll(&response[0], response_length);
  }
}

void RestServer::OnStatus() { ShareStatus(nullptr); }

RestServer::RestServer(uint16_t port, Player& player)
    : server_(port), web_socket_("/jazzlights-websocket", this), player_(player) {}

void RestServer::Start() {
  if (started_) { return; }
  started_ = true;
  // Note that mdns_init() returns ESP_ERR_INVALID_STATE if called before Wi-Fi is initialized (Wi-Fi needs
  // to be initialized by calling WiFi.begin() first but not necessarily connected).
  esp_err_t espRes = mdns_init();
  if (espRes != ESP_OK) { jll_fatal("Failed to initialize mDNS, error 0x%X: %s", espRes, esp_err_to_name(espRes)); }
  espRes = mdns_hostname_set("jazzlights-clouds");
  if (espRes != ESP_OK) { jll_fatal("Failed to set mDNS host name, error 0x%X: %s", espRes, esp_err_to_name(espRes)); }

  jll_info("Initializing RestServer");

  Player* player = &player_;
  player->set_status_watcher(this);

  server_.onNotFound(sNotFoundHandler);

  web_socket_.onEvent(WebSocket::EventHandler);
  server_.addHandler(&web_socket_);

  server_.begin();
  jll_info("Initialized RestServer");
}

}  // namespace jazzlights

#endif  // JL_REST_SERVER
