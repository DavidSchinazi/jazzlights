#include "jazzlights/networks/arduino_esp_wifi.h"

#if JAZZLIGHTS_ESP_WIFI

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

ArduinoEspWiFiNetwork::ArduinoEspWiFiNetwork(const char* ssid, const char* pass) : creds_{ssid, pass} {
  uint8_t macAddress[6] = {};
  localDeviceId_ = NetworkDeviceId(WiFi.macAddress(&macAddress[0]));
}

#ifdef ESP32
void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_READY: jll_info("Wi-Fi ready"); break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED: jll_info("Wi-Fi connected to \"%s\"", info.wifi_sta_connected.ssid); break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      jll_info("Wi-Fi got IP %u.%u.%u.%u", info.got_ip.ip_info.ip.addr & 0xFF,
               (info.got_ip.ip_info.ip.addr >> 8) & 0xFF, (info.got_ip.ip_info.ip.addr >> 16) & 0xFF,
               (info.got_ip.ip_info.ip.addr >> 24) & 0xFF);
      break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP: jll_info("Wi-Fi lost IP"); break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      jll_info("Wi-Fi disconnected reason=%u", info.wifi_sta_disconnected.reason);
      break;
    default: break;
  }
}
#endif  // ESP32

NetworkStatus ArduinoEspWiFiNetwork::update(NetworkStatus status, Milliseconds currentTime) {
  const wl_status_t newWiFiStatus = WiFi.status();
  if (newWiFiStatus != currentWiFiStatus_) {
    jll_info("%u %s Wi-Fi status changing from %s to %s", currentTime, networkName(),
             WiFiStatusToString(currentWiFiStatus_).c_str(), WiFiStatusToString(newWiFiStatus).c_str());
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
    jll_info("%u %s Wi-Fi giving up on DHCP, using %u.%u.%u.%u", currentTime, networkName(), ip[0], ip[1], ip[2],
             ip[3]);
    WiFi.config(ip, gw, snm);
  } else if (staticConf_ == nullptr && !attemptingDhcp_ && timeOfLastWiFiStatusTransition_ >= 0 &&
             currentTime - timeOfLastWiFiStatusTransition_ > kDhcpRetryTime) {
    attemptingDhcp_ = true;
    jll_info("%u %s Wi-Fi going back to another DHCP attempt", currentTime, networkName());
    WiFi.config(IPAddress(), IPAddress(), IPAddress());
    WiFi.reconnect();
  }
  switch (status) {
    case INITIALIZING: {
      jll_info("%u %s Wi-Fi " DEVICE_ID_FMT " connecting to %s...", currentTime, networkName(),
               DEVICE_ID_HEX(localDeviceId_), creds_.ssid);
      if (staticConf_) {
        IPAddress ip, gw, snm;
        ip.fromString(staticConf_->ip);
        gw.fromString(staticConf_->gateway);
        snm.fromString(staticConf_->subnetMask);
        WiFi.config(ip, gw, snm);
      }
#ifdef ESP32
      WiFi.onEvent(WiFiEvent, ARDUINO_EVENT_WIFI_READY);
      WiFi.onEvent(WiFiEvent, ARDUINO_EVENT_WIFI_STA_CONNECTED);
      WiFi.onEvent(WiFiEvent, ARDUINO_EVENT_WIFI_STA_GOT_IP);
      WiFi.onEvent(WiFiEvent, ARDUINO_EVENT_WIFI_STA_LOST_IP);
      WiFi.onEvent(WiFiEvent, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
#endif  // ESP32
      wl_status_t beginWiFiStatus = WiFi.begin(creds_.ssid, creds_.pass);
      jll_info("%u %s Wi-Fi begin to %s returned %s", currentTime, networkName(), creds_.ssid,
               WiFiStatusToString(beginWiFiStatus).c_str());
      return CONNECTING;
    } break;
    case CONNECTING: {
      switch (newWiFiStatus) {
        case WL_NO_SHIELD:
          jll_error("%u %s connection to %s failed: there's no WiFi shield", currentTime, networkName(), creds_.ssid);
          return CONNECTION_FAILED;

        case WL_NO_SSID_AVAIL:
          jll_error("%u %s connection to %s failed: SSID not available", currentTime, networkName(), creds_.ssid);
          return CONNECTION_FAILED;

        case WL_SCAN_COMPLETED:
          jll_debug("%u %s scan completed, still connecting to %s...", currentTime, networkName(), creds_.ssid);
          break;

        case WL_CONNECTED: {
          IPAddress mcaddr;
          mcaddr.fromString(mcastAddr_);
          int res = udp_.beginMulticast(mcaddr, port_);
          if (!res) {
            jll_error("%u %s can't begin multicast on port %d, multicast group %s", currentTime, networkName(), port_,
                      mcastAddr_);
            goto err;
          }
          IPAddress ip = WiFi.localIP();
          jll_info("%u %s connected to %s, IP: %d.%d.%d.%d, bound to port %d, multicast group: %s", currentTime,
                   networkName(), creds_.ssid, ip[0], ip[1], ip[2], ip[3], port_, mcastAddr_);
          return CONNECTED;
        } break;

        case WL_CONNECT_FAILED:
          jll_error("%u %s connection to %s failed", currentTime, networkName(), creds_.ssid);
          return CONNECTION_FAILED;

        case WL_CONNECTION_LOST:
          jll_error("%u %s connection to %s lost", currentTime, networkName(), creds_.ssid);
          return INITIALIZING;

        case WL_DISCONNECTED:
        default: {
          static int32_t last_t = 0;
          if (currentTime - last_t > 5000) {
            if (newWiFiStatus == WL_DISCONNECTED) {
              jll_debug("%u %s still connecting...", currentTime, networkName());
            } else {
              jll_info("%u %s still connecting, unexpected status code %s", currentTime, networkName(),
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
        case WL_DISCONNECTED:
          jll_info("%u %s disconnected from %s", currentTime, networkName(), creds_.ssid);
          return DISCONNECTED;
        default:
          jll_info("%u %s disconnecting from %s...", currentTime, networkName(), creds_.ssid);
          WiFi.disconnect();
          break;
      }
    } break;
  }

  return status;

err:
  jll_error("%u %s connection to %s failed", currentTime, networkName(), creds_.ssid);
  WiFi.disconnect();
  return CONNECTION_FAILED;
}

int ArduinoEspWiFiNetwork::recv(void* buf, size_t bufsize, std::string* details) {
  int cb = udp_.parsePacket();
  if (cb <= 0) {
    jll_debug("ArduinoEspWiFiNetwork::recv returned %d status = %s", cb, NetworkStatusToString(status()).c_str());
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

std::string ArduinoEspWiFiNetwork::getStatusStr(Milliseconds currentTime) const {
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
      snprintf(statStr, sizeof(statStr) - 1, "%s %u.%u.%u.%u - %ums", creds_.ssid, ip[0], ip[1], ip[2], ip[3],
               (lastRcv >= 0 ? currentTime - lastRcv : -1));
      return std::string(statStr);
    }
  }
  return "error";
}

// static
ArduinoEspWiFiNetwork* ArduinoEspWiFiNetwork::get() {
  static ArduinoEspWiFiNetwork sSingleton(JL_WIFI_SSID, JL_WIFI_PASSWORD);
  return &sSingleton;
}

}  // namespace jazzlights

#endif  // JAZZLIGHTS_ESP_WIFI
