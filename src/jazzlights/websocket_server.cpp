#include "jazzlights/websocket_server.h"

#if JL_WEBSOCKET_SERVER

#include <ESPAsyncWebServer.h>
#include <mdns.h>

#include <optional>

#include "jazzlights/util/log.h"

namespace jazzlights {

namespace {
static void sNotFoundHandler(AsyncWebServerRequest* request) {
  jll_info("WebSocketServer got unexpected request for \"%s\"", request->url().c_str());
  request->send(404, "text/plain", String("Not found nope for ") + request->url());
}

unsigned int Id(AsyncWebSocketClient* client) {
  if (client == nullptr) { return 0; }
  return static_cast<unsigned int>(client->id());
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
  kStatusNextPattern = 7,
};

enum WSStatusFlag : uint8_t {
  kWSStatusFlagOn = 0x80,
  kWSStatusFlagColorOverride = 0x40,
};

// static
void WebSocketServer::WebSocket::EventHandler(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type,
                                              void* arg, uint8_t* data, size_t len) {
  WebSocket* webSocket = static_cast<WebSocket*>(server);
  switch (type) {
    case WS_EVT_CONNECT:
      jll_info("WebSocket client #%u connected from %s:%u", Id(client), client->remoteIP().toString().c_str(),
               client->remotePort());
      break;
    case WS_EVT_DISCONNECT: jll_info("WebSocket client #%u disconnected", Id(client)); break;
    case WS_EVT_DATA: {
      AwsFrameInfo* info = reinterpret_cast<AwsFrameInfo*>(arg);
      if (info->final && info->index == 0 && info->len == len) {
        jll_info("WebSocket received unfragmented %s message of length %llu from client #%u ",
                 ((info->opcode == WS_TEXT) ? "text" : "binary"), info->len, Id(client));
        webSocket->webSocketServer_->HandleMessage(client, data, len);
      } else {
        jll_info("WebSocket received fragmented %s message at index %llu of length %llu with%s FIN from client #%u",
                 ((info->opcode == WS_TEXT) ? "text" : "binary"), info->index, info->len, (info->final ? "" : "out"),
                 Id(client));
        // TODO buffer and handle fragmented messages.
      }
    } break;
    case WS_EVT_PONG:
      jll_info("WebSocket received pong of length %u: %s from  client #%u", len,
               (len ? reinterpret_cast<char*>(data) : "<empty>"), Id(client));
      break;
    case WS_EVT_ERROR:
      jll_info("WebSocket received error[%u]: %s from client #%u", *reinterpret_cast<uint16_t*>(arg),
               reinterpret_cast<char*>(data), Id(client));
      break;
  }
}

void WebSocketServer::HandleMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
  jll_info("Handling WebSocket message of length %zu first byte %u from client #%u", len, (len > 0 ? data[0] : 0),
           Id(client));
  ScopedUpdatePauser pauser(this);
  if (len == 0) { return; }
  switch (static_cast<WSType>(data[0])) {
    case WSType::kStatusRequest: {
      jll_info("Got WebSocket status request from client #%u", Id(client));
      ShareStatus(client);
    } break;
    case WSType::kStatusSetOn: {
      if (len < 2) {
        jll_error("WebSocket ignoring unexpectedly short set on status request of length %zu", len);
        break;
      }
      bool enabled = (data[1] & kWSStatusFlagOn) != 0;
      jll_info("Got turn %s WebSocket request from client #%u", (enabled ? "on" : "off"), Id(client));
      player_.SetEnabled(enabled);
      if (!enabled) {
        // Reset to default parameters when turned off.
        player_.SetBrightness(255);
        player_.DisableColorOverride();
      }
      ShareStatus(client);
    } break;
    case WSType::kStatusSetBrightness: {
      if (len < 2) {
        jll_error("WebSocket ignoring unexpectedly short set brightness request of length %zu", len);
        break;
      }
      uint8_t brightness = data[1];
      jll_info("Got WebSocket brightness=%u request from client #%u", brightness, Id(client));
      player_.SetEnabled(true);
      player_.SetBrightness(brightness);
      ShareStatus(client);
    } break;
    case WSType::kStatusSetColor: {
      if (len < 4) {
        jll_error("WebSocket ignoring unexpectedly short set color request of length %zu", len);
        break;
      }
      uint8_t r = data[1];
      uint8_t g = data[2];
      uint8_t b = data[3];
      jll_info("Got WebSocket color=%02x%02x%02x request from client #%u", r, g, b, Id(client));
      player_.SetEnabled(true);
      player_.EnableColorOverride(CRGB(r, g, b));
      ShareStatus(client);
    } break;
    case WSType::kStatusSetEffect: {
      jll_info("Got WebSocket effect request from client #%u", Id(client));
      player_.SetEnabled(true);
      player_.DisableColorOverride();
      ShareStatus(client);
    } break;
    case WSType::kStatusNextPattern: {
      jll_info("Got WebSocket CloudNextPatern request from client #%u", Id(client));
      player_.CloudNext();
      ShareStatus(client);
    } break;
  }
}

void WebSocketServer::PauseUpdates() {
  if (pausedUpdateState_ != PausedUpdateState::kOpen) { jll_fatal("Tried to double pause WebSocketServer updates"); }
  pausedUpdateState_ = PausedUpdateState::kPausedNoUpdate;
  jll_debug("Pausing WebSocketServer updates");
}

void WebSocketServer::ResumeUpdates() {
  PausedUpdateState previousState = pausedUpdateState_;
  pausedUpdateState_ = PausedUpdateState::kOpen;
  switch (previousState) {
    case PausedUpdateState::kOpen: jll_fatal("Tried to resume non-paused WebSocketServer updates"); break;
    case PausedUpdateState::kPausedNoUpdate: jll_debug("Resuming WebSocketServer updates with none pending"); break;
    case PausedUpdateState::kPausedUpdateOneClient:
      jll_debug("Resuming WebSocketServer updates with pending to client #%u", Id(clientToUpdate_));
      ShareStatus(clientToUpdate_);
      break;
    case PausedUpdateState::kPausedUpdateAllClients:
      jll_debug("Resuming WebSocketServer updates with pending to all clients");
      ShareStatus(nullptr);
      break;
    default:
      jll_fatal("Tried to resume WebSocketServer updates from unexpected state %d",
                static_cast<int>(pausedUpdateState_));
      break;
  }
  clientToUpdate_ = nullptr;
}

void WebSocketServer::ShareStatus(AsyncWebSocketClient* client) {
  if (pausedUpdateState_ != PausedUpdateState::kOpen) {
    if (client == nullptr) {
      pausedUpdateState_ = PausedUpdateState::kPausedUpdateAllClients;
      clientToUpdate_ = nullptr;
    } else if (pausedUpdateState_ == PausedUpdateState::kPausedNoUpdate) {
      pausedUpdateState_ = PausedUpdateState::kPausedUpdateOneClient;
      clientToUpdate_ = client;
    } else if (pausedUpdateState_ == PausedUpdateState::kPausedUpdateOneClient && clientToUpdate_ != client) {
      pausedUpdateState_ = PausedUpdateState::kPausedUpdateAllClients;
      clientToUpdate_ = nullptr;
    }
    jll_debug("Skipping WebSocket update to client #%u because we are paused", Id(client));
    return;
  }
  const std::optional<CRGB>& colorOverride = player_.colorOverride();
  if (colorOverride) {
    jll_info("WebSocket sending status %s brightness=%u color=%02x%02x%02x to client #%u",
             (player_.enabled() ? "on" : "off"), player_.brightness(), colorOverride->r, colorOverride->g,
             colorOverride->b, Id(client));
  } else {
    jll_info("WebSocket sending status %s brightness=%u to client #%u", (player_.enabled() ? "on" : "off"),
             player_.brightness(), Id(client));
  }
  uint8_t response[6] = {};
  size_t responseLength = 3;
  response[0] = static_cast<uint8_t>(WSType::kStatusShare);
  if (player_.enabled()) { response[1] |= kWSStatusFlagOn; }
  response[2] = player_.brightness();
  if (colorOverride) {
    response[1] |= kWSStatusFlagColorOverride;
    response[3] = colorOverride->r;
    response[4] = colorOverride->g;
    response[5] = colorOverride->b;
    responseLength += 3;
  }
  if (client != nullptr) {
    client->binary(&response[0], responseLength);
  } else {
    webSocket_.binaryAll(&response[0], responseLength);
  }
}

void WebSocketServer::OnStatus() { ShareStatus(nullptr); }

WebSocketServer::WebSocketServer(uint16_t port, Player& player)
    : server_(port), webSocket_("/jazzlights-websocket", this), player_(player) {}

void WebSocketServer::Start() {
  if (started_) { return; }
  started_ = true;
  // Note that mdns_init() returns ESP_ERR_INVALID_STATE if called before Wi-Fi is initialized (Wi-Fi needs
  // to be initialized by calling WiFi.begin() first but not necessarily connected).
  esp_err_t espRes = mdns_init();
  if (espRes != ESP_OK) { jll_fatal("Failed to initialize mDNS, error 0x%X: %s", espRes, esp_err_to_name(espRes)); }
  espRes = mdns_hostname_set("jazzlights-clouds");
  if (espRes != ESP_OK) { jll_fatal("Failed to set mDNS host name, error 0x%X: %s", espRes, esp_err_to_name(espRes)); }

  jll_info("Initialized mDNS, initializing WebSocketServer");

  Player* player = &player_;
  player->SetStatusWatcher(this);

  server_.onNotFound(sNotFoundHandler);

  webSocket_.onEvent(WebSocket::EventHandler);
  server_.addHandler(&webSocket_);

  server_.begin();
  jll_info("Initialized WebSocketServer");
}

}  // namespace jazzlights

#endif  // JL_WEBSOCKET_SERVER
