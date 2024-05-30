#include "jazzlights/rest_server.h"

#if JL_IS_CONFIG(CLOUDS)

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
  kTurnOn = 3,
  kTurnOff = 4,
};

enum WSStatusFlag : uint8_t {
  kWSStatusFlagOn = 0x80,
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
  jll_info("Handling message of length %zu from client #%u", len, client->id());
  if (len == 0) { return; }
  switch (static_cast<WSType>(data[0])) {
    case WSType::kStatusRequest: {
      jll_info("Got status request from client #%u", client->id());
      ShareStatus(client);
    } break;
    case WSType::kTurnOn: {
      if (len >= 2) {
        uint8_t brightness = data[1];
        jll_info("Got turn on request with brightness=%u from client #%u", brightness, client->id());
        player_.set_brightness(brightness);
      } else {
        jll_info("Got turn on request from client #%u", client->id());
      }
      player_.set_enabled(true);
      ShareStatus(client);
    } break;
    case WSType::kTurnOff: {
      jll_info("Got turn off request from client #%u", client->id());
      player_.set_enabled(false);
      ShareStatus(client);
    } break;
  }
}

void RestServer::ShareStatus(AsyncWebSocketClient* client) {
  uint8_t response[3] = {};
  response[0] = static_cast<uint8_t>(WSType::kStatusShare);
  if (player_.enabled()) { response[1] |= kWSStatusFlagOn; }
  response[2] = player_.brightness();
  if (client != nullptr) {
    client->binary(&response[0], sizeof(response));
  } else {
    web_socket_.binaryAll(&response[0], sizeof(response));
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
  constexpr const char* const enabled_path = "/jl-clouds-enabled";
  server_.on(enabled_path, HTTP_GET, [player](AsyncWebServerRequest* request) {
    const bool enabled = player->enabled();
    jll_info("RestServer got GET request for \"%s\", replying %s", request->url().c_str(),
             (enabled ? "true" : "false"));
    const char* response;
    if (enabled) {
      response = "{\"is_active\": \"true\"}";
    } else {
      response = "{\"is_active\": \"false\"}";
    }
    request->send(200, "application/json", response);
  });
  server_.on(
      enabled_path, HTTP_POST,
      /*onRequest=*/
      [player](AsyncWebServerRequest* request) {
        jll_info("RestServer got POST request for \"%s\" but not doing anything with it here", request->url().c_str());
        // request->send(200, "text/plain", "post");
      },
      /*onUpload=*/nullptr,
      /*onBody=*/
      [player](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        // TODO only do this if index == 0 and len == total
        char body[1024] = {};
        memcpy(body, data, std::min<size_t>(len, sizeof(body) - 1));
        jll_info("RestServer got body request for \"%s\", len=%zu index=%zu total=%zu data=\"%s\"",
                 request->url().c_str(), len, index, total, body);
        // TODO actually parse the JSON
        // Or even better just get rid of JSON entirely since we can have HomeAssistant send any string we want.
        if (strncmp(body, "{\"active\": \"false\"}", sizeof(body)) == 0) {
          jll_info("RestServer marking player as disabled");
          player->set_enabled(false);
        } else if (strncmp(body, "{\"active\": \"true\"}", sizeof(body)) == 0) {
          jll_info("RestServer marking player as enabled");
          player->set_enabled(true);
        }
        request->send(200);
      });
  server_.onNotFound(sNotFoundHandler);

  web_socket_.onEvent(WebSocket::EventHandler);
  server_.addHandler(&web_socket_);

  server_.begin();
  jll_info("Initialized RestServer");
}

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(CLOUDS)
