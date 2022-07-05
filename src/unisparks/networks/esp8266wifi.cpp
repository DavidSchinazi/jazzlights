#include "unisparks/networks/esp8266wifi.hpp"

#if UNISPARKS_ESP8266WIFI

#include <sstream>

#include "unisparks/util/log.hpp"
#include "unisparks/util/time.hpp"

namespace unisparks {

std::string Esp8266WiFi::WiFiStatusToString(wl_status_t status) {
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

Esp8266WiFi::Esp8266WiFi(const char* ssid, const char* pass) : creds_{ssid, pass} {
  uint8_t macAddress[6] = {};
  localDeviceId_ = NetworkDeviceId(WiFi.macAddress(&macAddress[0]));
}

NetworkStatus Esp8266WiFi::update(NetworkStatus status, Milliseconds currentTime) {
  const wl_status_t newWiFiStatus = WiFi.status();
  if (newWiFiStatus != currentWiFiStatus_) {
    info("%u %s Wi-Fi status changing from %s to %s",
          currentTime, name(), WiFiStatusToString(currentWiFiStatus_).c_str(),
          WiFiStatusToString(newWiFiStatus).c_str());
    currentWiFiStatus_ = newWiFiStatus;
    timeOfLastWiFiStatusTransition_ = currentTime;
  }
  static constexpr Milliseconds kDhcpTimeout = 5000;  // 5s.
  static constexpr Milliseconds kDhcpRetryTime = 600000;  // 10min.
  if (staticConf_ == nullptr &&
      attemptingDhcp_ &&
      currentWiFiStatus_ == WL_IDLE_STATUS &&
      timeOfLastWiFiStatusTransition_ >= 0 &&
      currentTime - timeOfLastWiFiStatusTransition_ > kDhcpTimeout) {
    attemptingDhcp_ = false;
    uint8_t ipByteC, ipByteD;
    do {
#ifdef ESP32
      const uint32_t randNum = esp_random();
#else
      const uint32_t randNum = rand();
#endif
      ipByteC = randNum & 0xFF;
      ipByteD = (randNum >> 8) & 0xFF;
    } while (ipByteC == 0 || ipByteC == 255);
    IPAddress ip, gw, snm;
    ip[0] = 169;
    ip[1] = 254;
    ip[2] = ipByteC;
    ip[3] = ipByteD;
    gw.fromString("169.254.0.0");
    snm.fromString("255.255.0.0");
    info("%u %s Wi-Fi giving up on DHCP, using %u.%u.%u.%u",
         currentTime, name(), ip[0], ip[1], ip[2], ip[3]);
    WiFi.config(ip, gw, snm);
  } else if (staticConf_ == nullptr &&
      !attemptingDhcp_ &&
      timeOfLastWiFiStatusTransition_ >= 0 &&
      currentTime - timeOfLastWiFiStatusTransition_ > kDhcpRetryTime) {
    attemptingDhcp_ = true;
    info("%u %s Wi-Fi going back to another DHCP attempt",
         currentTime, name());
    WiFi.config(IPAddress(), IPAddress(), IPAddress());
    WiFi.reconnect();
  }
  switch (status) {
  case INITIALIZING: {
    info("%u %s Wi-Fi " DEVICE_ID_FMT " connecting to %s...",
         currentTime, name(), DEVICE_ID_HEX(localDeviceId_), creds_.ssid);
    if (staticConf_) {
      IPAddress ip, gw, snm;
      ip.fromString(staticConf_->ip);
      gw.fromString(staticConf_->gateway);
      snm.fromString(staticConf_->subnetMask);
      WiFi.config(ip, gw, snm);
    }

    wl_status_t beginWiFiStatus = WiFi.begin(creds_.ssid, creds_.pass);
    info("%u %s Wi-Fi begin to %s returned %s",
         currentTime, name(), creds_.ssid, WiFiStatusToString(beginWiFiStatus).c_str());
    return CONNECTING;
  } break;
  case CONNECTING: {
    switch (newWiFiStatus) {
    case WL_NO_SHIELD:
      error("%u %s connection to %s failed: there's no WiFi shield",
            currentTime, name(), creds_.ssid);
      return CONNECTION_FAILED;

    case WL_NO_SSID_AVAIL:
      error("%u %s connection to %s failed: SSID not available",
            currentTime, name(), creds_.ssid);
      return CONNECTION_FAILED;

    case WL_SCAN_COMPLETED:
      debug("%u %s scan completed, still connecting to %s...",
            currentTime, name(), creds_.ssid);
      break;

    case WL_CONNECTED: {
      IPAddress mcaddr;
      mcaddr.fromString(mcastAddr_);
#if defined(ESP8266)
      int res = udp_.beginMulticast(WiFi.localIP(), mcaddr, port_);
#elif defined(ESP32)
      int res = udp_.beginMulticast(mcaddr, port_);
#endif
      if (!res) {
        error("%u %s can't begin multicast on port %d, multicast group %s",
              currentTime, name(), port_, mcastAddr_);
        goto err;
      }
      IPAddress ip = WiFi.localIP();
      info("%u %s connected to %s, IP: %d.%d.%d.%d, bound to port %d, multicast group: %s",
           currentTime, name(), creds_.ssid, ip[0], ip[1], ip[2], ip[3], port_, mcastAddr_);
      return CONNECTED;
    }
    break;

    case WL_CONNECT_FAILED:
      error("%u %s connection to %s failed", currentTime, name(), creds_.ssid);
      return CONNECTION_FAILED;

    case WL_CONNECTION_LOST:
      error("%u %s connection to %s lost", currentTime, name(), creds_.ssid);
      return INITIALIZING;

    case WL_DISCONNECTED:
    default: {
      static int32_t last_t = 0;
      if (currentTime - last_t > 5000) {
        if (newWiFiStatus == WL_DISCONNECTED) {
          debug("%u %s still connecting...", currentTime, name());
        } else {
          info("%u %s still connecting, unexpected status code %s",
               currentTime, name(), WiFiStatusToString(newWiFiStatus).c_str());
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
      info("%u %s disconnected from %s", currentTime, name(), creds_.ssid);
      return DISCONNECTED;
    default:
      info("%u %s disconnecting from %s...", currentTime, name(), creds_.ssid);
      WiFi.disconnect();
      break;
    }
  } break;
  }

  return status;

err:
  error("%u %s connection to %s failed", currentTime, name(), creds_.ssid);
  WiFi.disconnect();
  return CONNECTION_FAILED;
}

int Esp8266WiFi::recv(void* buf, size_t bufsize, std::string* details) {
  int cb = udp_.parsePacket();
  if (cb <= 0) {
    debug("Esp8266WiFi::recv returned %d status = %s",
          cb, NetworkStatusToString(status()).c_str());
    return 0;
  }
  std::ostringstream s;
  s << " (from " << udp_.remoteIP().toString().c_str() << ":" << udp_.remotePort() << ")";
  *details = s.str();
  return udp_.read((unsigned char*)buf, bufsize);
}

#ifndef ENABLE_ESP_WIFI_SENDING
#  define ENABLE_ESP_WIFI_SENDING 1
#endif // ENABLE_ESP_WIFI_SENDING

void Esp8266WiFi::send(void* buf, size_t bufsize) {
#if ENABLE_ESP_WIFI_SENDING
  IPAddress ip;
  ip.fromString(mcastAddr_);
  udp_.beginPacket(ip, port_);
  udp_.write(static_cast<uint8_t*>(buf), bufsize);
  udp_.endPacket();
#else // ENABLE_ESP_WIFI_SENDING
  (void)buf;
  (void)bufsize;
#endif // ENABLE_ESP_WIFI_SENDING
}


} // namespace unisparks

#endif // UNISPARKS_ESP8266WIFI
