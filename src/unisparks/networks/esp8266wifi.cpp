#include "unisparks/networks/esp8266wifi.hpp"

#if UNISPARKS_ESP8266WIFI

#include "unisparks/util/log.hpp"
#include "unisparks/util/time.hpp"

namespace unisparks {

Esp8266WiFi::Esp8266WiFi(const char* ssid, const char* pass) : creds_{ssid, pass} {
  uint8_t macAddress[6] = {};
  localDeviceId_ = NetworkDeviceId(WiFi.macAddress(&macAddress[0]));
  info("%s Wi-Fi MAC is " DEVICE_ID_FMT, name(), DEVICE_ID_HEX(localDeviceId_));
}

NetworkStatus Esp8266WiFi::update(NetworkStatus status, Milliseconds currentTime) {
  switch (status) {
  case INITIALIZING: {
  info("%u %s Wi-Fi connecting to %s...", currentTime, name(), creds_.ssid);
    if (staticConf_) {
      IPAddress ip, gw, snm;
      ip.fromString(staticConf_->ip);
      gw.fromString(staticConf_->gateway);
      snm.fromString(staticConf_->subnetMask);
      WiFi.config(ip, gw, snm);
    }

    // TODO figure out why first connection fails with missing Wi-Fi shield.
    WiFi.begin(creds_.ssid, creds_.pass);
    return CONNECTING;
  } break;
  case CONNECTING: {
    switch (WiFi.status()) {
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
        int st = WiFi.status();
        if (st == WL_DISCONNECTED) {
          debug("%u %s still connecting...", currentTime, name());
        } else {
          info("%u %s still connecting, unknown status code %d", currentTime, name(), st);
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
    switch (WiFi.status()) {
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

int Esp8266WiFi::recv(void* buf, size_t bufsize) {
  int cb = udp_.parsePacket();
  if (cb <= 0) {
    debug("Esp8266WiFi::recv returned %d status = %s",
          cb, NetworkStatusToString(status()).c_str());
    return 0;
  }
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
