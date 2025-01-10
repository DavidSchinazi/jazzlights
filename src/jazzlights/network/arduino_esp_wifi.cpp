#include "jazzlights/network/arduino_esp_wifi.h"

#if JL_WIFI
#if JL_ESP_WIFI

#include <sstream>

#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

namespace jazzlights {
std::string ArduinoEspWiFiNetwork::WiFiStatusToString(wl_status_t status) {
  if (status == kUninitialized) { return "UNINITIALIZED"; }
  switch (status) {
    case WL_NO_SHIELD: return "WL_NO_SHIELD";
    case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
    case WL_CONNECTED: return "WL_CONNECTED";
    case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED: return "WL_DISCONNECTED";
  }
  std::ostringstream s;
  s << "UNKNOWN(" << static_cast<int>(status) << ")";
  return s.str();
}

namespace {
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
  std::ostringstream s;
  s << "UNKNOWN(" << static_cast<int>(reason) << ")";
  return s.str();
}
}  // namespace

// static
NetworkDeviceId ArduinoEspWiFiNetwork::QueryLocalDeviceId() {
  uint8_t macAddress[6] = {};
  if (WiFi.macAddress(&macAddress[0]) == nullptr) {
    uint64_t efuseMac64 = ESP.getEfuseMac();
    if (efuseMac64 == 0) { jll_fatal("%u Wi-Fi failed to read MAC address from both Wi-Fi and EFUSE", timeMillis()); }
    memcpy(macAddress, &efuseMac64, sizeof(macAddress));
  }
  return NetworkDeviceId(macAddress);
}

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_READY: jll_info("%u Wi-Fi ready", timeMillis()); break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      jll_info("%u Wi-Fi connected to \"%s\"", timeMillis(), info.wifi_sta_connected.ssid);
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      jll_info("%u Wi-Fi got IP " IPSTR, timeMillis(), IP2STR(&info.got_ip.ip_info.ip));
      break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP: jll_info("%u Wi-Fi lost IP", timeMillis()); break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      jll_info("%u Wi-Fi disconnected reason=%s", timeMillis(),
               WiFiReasonToString(info.wifi_sta_disconnected.reason).c_str());
      break;
    default: break;
  }
}

NetworkStatus ArduinoEspWiFiNetwork::update(NetworkStatus status, Milliseconds currentTime) {
  const wl_status_t newWiFiStatus = WiFi.status();
  if (newWiFiStatus != currentWiFiStatus_) {
    jll_info("%u Wi-Fi status changing from %s to %s", currentTime, WiFiStatusToString(currentWiFiStatus_).c_str(),
             WiFiStatusToString(newWiFiStatus).c_str());
    currentWiFiStatus_ = newWiFiStatus;
    timeOfLastWiFiStatusTransition_ = currentTime;
  }
  static constexpr Milliseconds kDhcpTimeout = 5000;      // 5s.
  static constexpr Milliseconds kDhcpRetryTime = 600000;  // 10min.
  if (staticConf_ == nullptr && attemptingDhcp_ && currentWiFiStatus_ == WL_IDLE_STATUS &&
      timeOfLastWiFiStatusTransition_ >= 0 && currentTime - timeOfLastWiFiStatusTransition_ > kDhcpTimeout) {
    attemptingDhcp_ = false;
    IPAddress ip, gw, snm;
    ip[0] = 169;
    ip[1] = 254;
    ip[2] = UnpredictableRandom::GetNumberBetween(1, 254);
    ip[3] = UnpredictableRandom::GetNumberBetween(0, 255);
    gw.fromString("169.254.0.0");
    snm.fromString("255.255.0.0");
    jll_info("%u Wi-Fi giving up on DHCP, using %u.%u.%u.%u", currentTime, ip[0], ip[1], ip[2], ip[3]);
    WiFi.config(ip, gw, snm);
  } else if (staticConf_ == nullptr && !attemptingDhcp_ && timeOfLastWiFiStatusTransition_ >= 0 &&
             currentTime - timeOfLastWiFiStatusTransition_ > kDhcpRetryTime) {
    attemptingDhcp_ = true;
    jll_info("%u Wi-Fi going back to another DHCP attempt", currentTime);
    WiFi.config(IPAddress(), IPAddress(), IPAddress());
    WiFi.reconnect();
  }
  switch (status) {
    case INITIALIZING: {
      jll_info("%u Wi-Fi connecting to %s...", currentTime, WiFiSsid());
      if (staticConf_) {
        IPAddress ip, gw, snm;
        ip.fromString(staticConf_->ip);
        gw.fromString(staticConf_->gateway);
        snm.fromString(staticConf_->subnetMask);
        WiFi.config(ip, gw, snm);
      }
      WiFi.onEvent(WiFiEvent, ARDUINO_EVENT_WIFI_READY);
      WiFi.onEvent(WiFiEvent, ARDUINO_EVENT_WIFI_STA_CONNECTED);
      WiFi.onEvent(WiFiEvent, ARDUINO_EVENT_WIFI_STA_GOT_IP);
      WiFi.onEvent(WiFiEvent, ARDUINO_EVENT_WIFI_STA_LOST_IP);
      WiFi.onEvent(WiFiEvent, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
      wl_status_t beginWiFiStatus = WiFi.begin(WiFiSsid(), WiFiPassword());
      jll_info("%u Wi-Fi " DEVICE_ID_FMT " begin to %s returned %s", currentTime, DEVICE_ID_HEX(localDeviceId_),
               WiFiSsid(), WiFiStatusToString(beginWiFiStatus).c_str());
      return CONNECTING;
    } break;
    case CONNECTING: {
      switch (newWiFiStatus) {
        case WL_NO_SHIELD:
          jll_error("%u Wi-Fi connection to %s failed: there's no WiFi shield", currentTime, WiFiSsid());
          return CONNECTION_FAILED;

        case WL_NO_SSID_AVAIL:
          jll_error("%u Wi-Fi connection to %s failed: SSID not available", currentTime, WiFiSsid());
          return CONNECTION_FAILED;

        case WL_SCAN_COMPLETED:
          jll_debug("%u Wi-Fi scan completed, still connecting to %s...", currentTime, WiFiSsid());
          break;

        case WL_CONNECTED: {
          IPAddress mcaddr;
          mcaddr.fromString(mcastAddr_);
          int res = udp_.beginMulticast(mcaddr, port_);
          if (!res) {
            jll_error("%u Wi-Fi can't begin multicast on port %u, multicast group %s", currentTime, port_, mcastAddr_);
            goto err;
          }
          IPAddress ip = WiFi.localIP();
          jll_info("%u Wi-Fi connected to %s, IP: %d.%d.%d.%d, bound to port %u, multicast group: %s", currentTime,
                   WiFiSsid(), ip[0], ip[1], ip[2], ip[3], port_, mcastAddr_);
          return CONNECTED;
        } break;

        case WL_CONNECT_FAILED:
          jll_error("%u Wi-Fi connection to %s failed", currentTime, WiFiSsid());
          return CONNECTION_FAILED;

        case WL_CONNECTION_LOST:
          jll_error("%u Wi-Fi connection to %s lost", currentTime, WiFiSsid());
          return INITIALIZING;

        case WL_DISCONNECTED:
        default: {
          static int32_t last_t = 0;
          if (currentTime - last_t > 5000) {
            if (newWiFiStatus == WL_DISCONNECTED) {
              jll_debug("%u Wi-Fi still connecting...", currentTime);
            } else {
              jll_info("%u Wi-Fi still connecting, unexpected status code %s", currentTime,
                       WiFiStatusToString(newWiFiStatus).c_str());
            }
            last_t = currentTime;
          }
        }
      }
    } break;

    case CONNECTED:
    case DISCONNECTED:
    case CONNECTION_FAILED:
      // do nothing
      break;

    case DISCONNECTING: {
      switch (newWiFiStatus) {
        case WL_DISCONNECTED: jll_info("%u Wi-Fi disconnected from %s", currentTime, WiFiSsid()); return DISCONNECTED;
        default:
          jll_info("%u Wi-Fi disconnecting from %s...", currentTime, WiFiSsid());
          WiFi.disconnect();
          break;
      }
    } break;
  }

  return status;

err:
  jll_error("%u Wi-Fi connection to %s failed", currentTime, WiFiSsid());
  WiFi.disconnect();
  return CONNECTION_FAILED;
}

int ArduinoEspWiFiNetwork::recv(void* buf, size_t bufsize, std::string* details) {
  int cb = udp_.parsePacket();
  if (cb <= 0) {
    jll_debug("%u ArduinoEspWiFiNetwork::recv returned %d status = %s", timeMillis(), cb,
              NetworkStatusToString(status()).c_str());
    return 0;
  }
  std::ostringstream s;
  s << " (from " << udp_.remoteIP().toString().c_str() << ":" << udp_.remotePort() << ")";
  *details = s.str();
  return udp_.read((unsigned char*)buf, bufsize);
}

#ifndef ENABLE_ESP_WIFI_SENDING
#define ENABLE_ESP_WIFI_SENDING 1
#endif  // ENABLE_ESP_WIFI_SENDING

void ArduinoEspWiFiNetwork::send(void* buf, size_t bufsize) {
#if ENABLE_ESP_WIFI_SENDING
  IPAddress ip;
  ip.fromString(mcastAddr_);
  udp_.beginPacket(ip, port_);
  udp_.write(static_cast<uint8_t*>(buf), bufsize);
  udp_.endPacket();
#else   // ENABLE_ESP_WIFI_SENDING
  (void)buf;
  (void)bufsize;
#endif  // ENABLE_ESP_WIFI_SENDING
}

std::string ArduinoEspWiFiNetwork::getStatusStr(Milliseconds currentTime) {
  switch (getStatus()) {
    case INITIALIZING: return "init";
    case DISCONNECTED: return "disconnected";
    case CONNECTING: return "connecting";
    case DISCONNECTING: return "disconnecting";
    case CONNECTION_FAILED: return "failed";
    case CONNECTED: {
      IPAddress ip = WiFi.localIP();
      const Milliseconds lastRcv = getLastReceiveTime();
      char statStr[100] = {};
      snprintf(statStr, sizeof(statStr) - 1, "%s %u.%u.%u.%u - %ums", WiFiSsid(), ip[0], ip[1], ip[2], ip[3],
               (lastRcv >= 0 ? currentTime - lastRcv : -1));
      return std::string(statStr);
    }
  }
  return "error";
}

// static
ArduinoEspWiFiNetwork* ArduinoEspWiFiNetwork::get() {
  static ArduinoEspWiFiNetwork sSingleton;
  return &sSingleton;
}

}  // namespace jazzlights

#endif  // JL_ESP_WIFI
#endif  // JL_WIFI
