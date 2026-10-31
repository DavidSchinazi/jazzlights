#include "jazzlights/network/esp32_wifi_link.h"

#if JL_WIFI

#include <esp_event.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <string.h>

#include <algorithm>
#include <string>

#include "jazzlights/network/network.h"
#include "jazzlights/util/esp32_shared.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

namespace jazzlights {
namespace {
constexpr uint32_t kNumReconnectsBeforeDelay = 10;

std::string WiFiReasonToString(uint8_t reason) {
  switch (reason) {
#define JL_WIFI_REASON_CASE_RETURN(r) \
  case WIFI_REASON_##r: return #r;
    JL_WIFI_REASON_CASE_RETURN(UNSPECIFIED)
    JL_WIFI_REASON_CASE_RETURN(AUTH_EXPIRE)
    JL_WIFI_REASON_CASE_RETURN(AUTH_LEAVE)
    JL_WIFI_REASON_CASE_RETURN(ASSOC_EXPIRE)
    JL_WIFI_REASON_CASE_RETURN(ASSOC_TOOMANY)
    JL_WIFI_REASON_CASE_RETURN(NOT_AUTHED)
    JL_WIFI_REASON_CASE_RETURN(NOT_ASSOCED)
    JL_WIFI_REASON_CASE_RETURN(ASSOC_LEAVE)
    JL_WIFI_REASON_CASE_RETURN(ASSOC_NOT_AUTHED)
    JL_WIFI_REASON_CASE_RETURN(DISASSOC_PWRCAP_BAD)
    JL_WIFI_REASON_CASE_RETURN(DISASSOC_SUPCHAN_BAD)
    JL_WIFI_REASON_CASE_RETURN(BSS_TRANSITION_DISASSOC)
    JL_WIFI_REASON_CASE_RETURN(IE_INVALID)
    JL_WIFI_REASON_CASE_RETURN(MIC_FAILURE)
    JL_WIFI_REASON_CASE_RETURN(4WAY_HANDSHAKE_TIMEOUT)
    JL_WIFI_REASON_CASE_RETURN(GROUP_KEY_UPDATE_TIMEOUT)
    JL_WIFI_REASON_CASE_RETURN(IE_IN_4WAY_DIFFERS)
    JL_WIFI_REASON_CASE_RETURN(GROUP_CIPHER_INVALID)
    JL_WIFI_REASON_CASE_RETURN(PAIRWISE_CIPHER_INVALID)
    JL_WIFI_REASON_CASE_RETURN(AKMP_INVALID)
    JL_WIFI_REASON_CASE_RETURN(UNSUPP_RSN_IE_VERSION)
    JL_WIFI_REASON_CASE_RETURN(INVALID_RSN_IE_CAP)
    JL_WIFI_REASON_CASE_RETURN(802_1X_AUTH_FAILED)
    JL_WIFI_REASON_CASE_RETURN(CIPHER_SUITE_REJECTED)
    JL_WIFI_REASON_CASE_RETURN(TDLS_PEER_UNREACHABLE)
    JL_WIFI_REASON_CASE_RETURN(TDLS_UNSPECIFIED)
    JL_WIFI_REASON_CASE_RETURN(SSP_REQUESTED_DISASSOC)
    JL_WIFI_REASON_CASE_RETURN(NO_SSP_ROAMING_AGREEMENT)
    JL_WIFI_REASON_CASE_RETURN(BAD_CIPHER_OR_AKM)
    JL_WIFI_REASON_CASE_RETURN(NOT_AUTHORIZED_THIS_LOCATION)
    JL_WIFI_REASON_CASE_RETURN(SERVICE_CHANGE_PERCLUDES_TS)
    JL_WIFI_REASON_CASE_RETURN(UNSPECIFIED_QOS)
    JL_WIFI_REASON_CASE_RETURN(NOT_ENOUGH_BANDWIDTH)
    JL_WIFI_REASON_CASE_RETURN(MISSING_ACKS)
    JL_WIFI_REASON_CASE_RETURN(EXCEEDED_TXOP)
    JL_WIFI_REASON_CASE_RETURN(STA_LEAVING)
    JL_WIFI_REASON_CASE_RETURN(END_BA)
    JL_WIFI_REASON_CASE_RETURN(UNKNOWN_BA)
    JL_WIFI_REASON_CASE_RETURN(TIMEOUT)
    JL_WIFI_REASON_CASE_RETURN(PEER_INITIATED)
    JL_WIFI_REASON_CASE_RETURN(AP_INITIATED)
    JL_WIFI_REASON_CASE_RETURN(INVALID_FT_ACTION_FRAME_COUNT)
    JL_WIFI_REASON_CASE_RETURN(INVALID_PMKID)
    JL_WIFI_REASON_CASE_RETURN(INVALID_MDE)
    JL_WIFI_REASON_CASE_RETURN(INVALID_FTE)
    JL_WIFI_REASON_CASE_RETURN(TRANSMISSION_LINK_ESTABLISH_FAILED)
    JL_WIFI_REASON_CASE_RETURN(ALTERATIVE_CHANNEL_OCCUPIED)
    JL_WIFI_REASON_CASE_RETURN(BEACON_TIMEOUT)
    JL_WIFI_REASON_CASE_RETURN(NO_AP_FOUND)
    JL_WIFI_REASON_CASE_RETURN(AUTH_FAIL)
    JL_WIFI_REASON_CASE_RETURN(ASSOC_FAIL)
    JL_WIFI_REASON_CASE_RETURN(HANDSHAKE_TIMEOUT)
    JL_WIFI_REASON_CASE_RETURN(CONNECTION_FAIL)
    JL_WIFI_REASON_CASE_RETURN(AP_TSF_RESET)
    JL_WIFI_REASON_CASE_RETURN(ROAMING)
    JL_WIFI_REASON_CASE_RETURN(ASSOC_COMEBACK_TIME_TOO_LONG)
    JL_WIFI_REASON_CASE_RETURN(SA_QUERY_TIMEOUT)
#undef JL_WIFI_REASON_CASE_RETURN
  }
  char unknownStr[sizeof("UNKNOWN(-2147483648)")] = {};
  snprintf(unknownStr, sizeof(unknownStr), "UNKNOWN(%d)", static_cast<int>(reason));
  return std::string(unknownStr);
}
}  // namespace

// static
void Esp32WiFiLink::EventHandler(void* eventHandlerArg, esp_event_base_t eventBase, int32_t eventId, void* eventData) {
  Esp32WiFiLink* wifiLink = reinterpret_cast<Esp32WiFiLink*>(eventHandlerArg);
  wifiLink->HandleEvent(eventBase, eventId, eventData);
}

void Esp32WiFiLink::HandleEvent(esp_event_base_t eventBase, int32_t eventId, void* eventData) {
  if (eventBase == WIFI_EVENT) {
    if (eventId == WIFI_EVENT_STA_START) {
      jll_info("Esp32WiFiLink STA started - connecting");
      esp_wifi_connect();
    } else if (eventId == WIFI_EVENT_STA_CONNECTED) {
      jll_info("Esp32WiFiLink STA connected");
    } else if (eventId == WIFI_EVENT_STA_DISCONNECTED) {
      wifi_event_sta_disconnected_t* event = reinterpret_cast<wifi_event_sta_disconnected_t*>(eventData);
      reconnectCount_++;
      if (reconnectCount_ < kNumReconnectsBeforeDelay) {
        jll_info("Esp32WiFiLink STA disconnected (%s, count %lld) - reconnecting immediately",
                 WiFiReasonToString(event->reason).c_str(), static_cast<long long>(reconnectCount_));
        esp_wifi_connect();
      } else {
        // Backoff exponentially from 1s to 32s.
        const uint64_t delaySeconds = 1ull << std::min<uint32_t>(reconnectCount_ - kNumReconnectsBeforeDelay, 5);
        jll_info("Esp32WiFiLink STA disconnected (%s, count %lld) - reconnecting in %llds",
                 WiFiReasonToString(event->reason).c_str(), static_cast<long long>(reconnectCount_),
                 static_cast<long long>(delaySeconds));
        // Stopping fails harmlessly when the timer isn't running.
        (void)esp_timer_stop(reconnectTimer_);
        ESP_ERROR_CHECK(esp_timer_start_once(reconnectTimer_, delaySeconds * kMicrosecondsPerSecond));
      }
    }
  } else if (eventBase == IP_EVENT) {
    if (eventId == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t* event = reinterpret_cast<ip_event_got_ip_t*>(eventData);
      jll_info("Esp32WiFiLink got IP: " IPSTR, IP2STR(&event->ip_info.ip));
      reconnectCount_ = 0;
    } else if (eventId == IP_EVENT_STA_LOST_IP) {
      jll_info("Esp32WiFiLink lost IP");
    }
  }
}

// static
void Esp32WiFiLink::ReconnectTimerCallback(void* /*arg*/) {
  jll_info("Esp32WiFiLink reconnection timer fired - reconnecting");
  esp_wifi_connect();
}

NetworkDeviceId Esp32WiFiLink::InitWiFiStackAndQueryLocalDeviceId() {
  InitializeNetStack();
  netif_ = esp_netif_create_default_wifi_sta();
  if (netif_ == nullptr) { jll_fatal("Esp32WiFiLink failed to create Wi-Fi STA netif"); }

  const esp_timer_create_args_t reconnectTimerArgs = {
      .callback = &ReconnectTimerCallback,
      .arg = this,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "JL_WiFiReconnect",
      .skip_unhandled_events = true,
  };
  ESP_ERROR_CHECK(esp_timer_create(&reconnectTimerArgs, &reconnectTimer_));

  wifi_init_config_t wifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifiInitConfig));

  esp_event_handler_instance_t wifiInstance;
  esp_event_handler_instance_t ipInstance;
  // Register event handlers on the default event loop. That runs in task "sys_evt" on core 0 (and is not
  // configurable).
  ESP_ERROR_CHECK(
      esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &EventHandler, this, &wifiInstance));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &EventHandler, this, &ipInstance));

  wifi_config_t wifiConfig;
  memset(&wifiConfig, 0, sizeof(wifiConfig));
  wifiConfig.sta = {};
  strncpy(reinterpret_cast<char*>(wifiConfig.sta.ssid), Network::WiFiSsid(), sizeof(wifiConfig.sta.ssid) - 1);
  strncpy(reinterpret_cast<char*>(wifiConfig.sta.password), Network::WiFiPassword(),
          sizeof(wifiConfig.sta.password) - 1);
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));
  ESP_ERROR_CHECK(esp_wifi_start());

  uint8_t macAddress[6];
  ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, macAddress));
  return NetworkDeviceId(macAddress);
}

Esp32WiFiLink::Esp32WiFiLink() {
  jll_info("Esp32WiFiLink initialized Wi-Fi STA with MAC address " DEVICE_ID_FMT " for SSID %s",
           DEVICE_ID_HEX(localDeviceId_), Network::WiFiSsid());
}

// static
Esp32WiFiLink* Esp32WiFiLink::Get() {
  static Esp32WiFiLink sSingleton;
  return &sSingleton;
}

Esp32WiFiLink::~Esp32WiFiLink() {
  jll_fatal("Destructing Esp32WiFiLink is not currently supported");
  // This destruction is unsafe since a race condition could cause the event handler to fire after the destructor is
  // called. If we ever have a need to destroy this, we'll need to make this safe first.
}

}  // namespace jazzlights

#endif  // JL_WIFI
