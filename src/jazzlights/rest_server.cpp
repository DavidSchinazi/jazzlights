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

static void sWebSocketEventHandler(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg,
                                   uint8_t* data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      jll_info("WebSocket client #%u connected from %s:%u", client->id(), client->remoteIP().toString().c_str(),
               client->remotePort());
      break;
    case WS_EVT_DISCONNECT: jll_info("WebSocket client #%u disconnected", client->id()); break;
    case WS_EVT_DATA: {
      AwsFrameInfo* info = reinterpret_cast<AwsFrameInfo*>(arg);
      if (info->final && info->index == 0 && info->len == len) {
        jll_info("WebSocket client #%u sent unfragmented %s message of length %llu", client->id(),
                 ((info->opcode == WS_TEXT) ? "text" : "binary"), info->len);
      } else {
        jll_info("WebSocket client #%u sent fragmented %s message at index %llu of length %llu with%s FIN",
                 client->id(), ((info->opcode == WS_TEXT) ? "text" : "binary"), info->index, info->len,
                 (info->final ? "" : "out"));
      }
    } break;
    case WS_EVT_PONG:
      jll_info("WebSocket client #%u sent us pong of length %u: %s", client->id(), len,
               (len ? reinterpret_cast<char*>(data) : "<empty>"));
      break;
    case WS_EVT_ERROR:
      jll_info("WebSocket client #%u sent us error[%u]: %s", client->id(), *reinterpret_cast<uint16_t*>(arg),
               reinterpret_cast<char*>(data));
      break;
  }
}
}  // namespace

RestServer::RestServer(uint16_t port, Player& player) : server_(port), web_socket_("/ws"), player_(player) {}

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

  web_socket_.onEvent(sWebSocketEventHandler);
  server_.addHandler(&web_socket_);

  server_.begin();
  jll_info("Initialized RestServer");
}

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(CLOUDS)
