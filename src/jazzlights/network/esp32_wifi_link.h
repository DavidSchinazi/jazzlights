#ifndef JL_NETWORK_ESP32_WIFI_LINK_H
#define JL_NETWORK_ESP32_WIFI_LINK_H

#include "jazzlights/util/config.h"

#if JL_WIFI

#include <esp_event.h>
#include <esp_netif.h>
#include <esp_timer.h>

#include <cstdint>

#include "jazzlights/protocol/wire_types.h"

namespace jazzlights {

// Manages the Wi-Fi station link: picks the SSID and password, connects, and reconnects with backoff when the
// connection fails. Sockets on top of this link are handled separately by Esp32SocketManager.
class Esp32WiFiLink {
 public:
  static Esp32WiFiLink* Get();
  ~Esp32WiFiLink();

  // Disallow copy and move.
  Esp32WiFiLink(const Esp32WiFiLink&) = delete;
  Esp32WiFiLink(Esp32WiFiLink&&) = delete;
  Esp32WiFiLink& operator=(const Esp32WiFiLink&) = delete;
  Esp32WiFiLink& operator=(Esp32WiFiLink&&) = delete;

  // The ESP-IDF network interface for the Wi-Fi station.
  esp_netif_t* GetNetif() const { return netif_; }
  NetworkDeviceId GetLocalDeviceId() const { return localDeviceId_; }

 private:
  explicit Esp32WiFiLink();
  static void EventHandler(void* eventHandlerArg, esp_event_base_t eventBase, int32_t eventId, void* eventData);
  void HandleEvent(esp_event_base_t eventBase, int32_t eventId, void* eventData);
  static void ReconnectTimerCallback(void* arg);
  NetworkDeviceId InitWiFiStackAndQueryLocalDeviceId();

  // These must be declared before localDeviceId_ since its initialization sets them and can fire events.
  esp_netif_t* netif_ = nullptr;                 // Only modified during construction.
  esp_timer_handle_t reconnectTimer_ = nullptr;  // Only modified during construction.
  uint32_t reconnectCount_ = 0;                  // Only used on the system event task.
  const NetworkDeviceId localDeviceId_ = InitWiFiStackAndQueryLocalDeviceId();
};

}  // namespace jazzlights

#endif  // JL_WIFI
#endif  // JL_NETWORK_ESP32_WIFI_LINK_H
