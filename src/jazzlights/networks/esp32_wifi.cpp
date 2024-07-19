#include "jazzlights/networks/esp32_wifi.h"

#ifdef ESP32
#if JL_ESP32_WIFI

#include <esp_wifi.h>

#include <sstream>

#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

namespace jazzlights {
namespace {
//
}  // namespace

NetworkStatus Esp32WiFiNetwork::update(NetworkStatus status, Milliseconds currentTime) {
  if (status == INITIALIZING || status == CONNECTING) {
    return CONNECTED;
  } else if (status == DISCONNECTING) {
    return DISCONNECTED;
  } else {
    return status;
  }
}

std::string Esp32WiFiNetwork::getStatusStr(Milliseconds currentTime) const { return "silly"; }

void Esp32WiFiNetwork::setMessageToSend(const NetworkMessage& messageToSend, Milliseconds /*currentTime*/) {
  const std::lock_guard<std::mutex> lock(mutex_);
  hasDataToSend_ = true;
  messageToSend_ = messageToSend;
}

void Esp32WiFiNetwork::disableSending(Milliseconds /*currentTime*/) {
  const std::lock_guard<std::mutex> lock(mutex_);
  hasDataToSend_ = false;
}

void Esp32WiFiNetwork::triggerSendAsap(Milliseconds /*currentTime*/) {}

std::list<NetworkMessage> Esp32WiFiNetwork::getReceivedMessagesImpl(Milliseconds /*currentTime*/) {
  std::list<NetworkMessage> results;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    results = std::move(receivedMessages_);
    receivedMessages_.clear();
  }
  return results;
}

// static
void Esp32WiFiNetwork::EventHandler(void* /*event_handler_arg*/, esp_event_base_t event_base, int32_t event_id,
                                    void* event_data) {
  if (event_base == WIFI_EVENT) {
    if (event_id == WIFI_EVENT_STA_START) {
      jll_info("Esp32WiFiNetwork STA started - connecting");
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
      jll_info("Esp32WiFiNetwork STA connected");
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
      jll_info("Esp32WiFiNetwork STA disconnected - reconnecting");
    }
    if (event_id == WIFI_EVENT_STA_START || event_id == WIFI_EVENT_STA_DISCONNECTED) { esp_wifi_connect(); }
  } else if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
      jll_info("Esp32WiFiNetwork got IP: " IPSTR, IP2STR(&event->ip_info.ip));
      // TODO register for receiving multicast
      // TODO receive packets
      // TODO send packets
    } else if (event_id == IP_EVENT_STA_LOST_IP) {
      jll_info("Esp32WiFiNetwork lost IP");
    } else if (event_id == IP_EVENT_GOT_IP6) {
      jll_info("Esp32WiFiNetwork got IPv6");
    }
  }
}

Esp32WiFiNetwork::Esp32WiFiNetwork() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
  // TODO should we set wifi_init_config.wifi_task_core_id to 1? The Arduino Core uses 0 and that's been working fine.
  ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(
      esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &EventHandler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(
      esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &EventHandler, NULL, &instance_got_ip));

  wifi_config_t wifi_config;
  memset(&wifi_config, 0, sizeof(wifi_config));
  wifi_config.sta = {};
  strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), JL_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
  strncpy(reinterpret_cast<char*>(wifi_config.sta.password), JL_WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);
  // TODO add support for IPv4 link-local addressing in the absence of DHCP. It looks like ESP-IDF supports it via
  // CONFIG_LWIP_AUTOIP but that doesn't seem to exist in our sdkconfig. I think that's because ESP-IDF disables it by
  // default, and the Arduino Core doesn't override that, so we'll need a custom ESP-IDF sdkconfig to enable it.
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  uint8_t macAddress[6];
  ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, macAddress));
  localDeviceId_ = NetworkDeviceId(macAddress);

  jll_info("Esp32WiFiNetwork initialized Wi-Fi STA with MAC address %s", localDeviceId_.toString().c_str());
}

// static
Esp32WiFiNetwork* Esp32WiFiNetwork::get() {
  static Esp32WiFiNetwork sSingleton;
  return &sSingleton;
}

}  // namespace jazzlights

#endif  // JL_ESP32_WIFI
#endif  // ESP32
