#include "unisparks/networks/esp8266wifi.hpp"

#if UNISPARKS_ESP8266WIFI

#include "unisparks/util/log.hpp"
#include "unisparks/util/time.hpp"

namespace unisparks {


NetworkStatus Esp8266WiFi::update(NetworkStatus status) {
  switch (status) {
  case INITIALIZING:
    info("Connecting to %s...", creds_.ssid);
    if (staticConf_) {
      IPAddress ip, gw, snm;
      ip.fromString(staticConf_->ip);
      gw.fromString(staticConf_->gateway);
      snm.fromString(staticConf_->subnetMask);
      WiFi.config(ip, gw, snm);
    }

    WiFi.begin(creds_.ssid, creds_.pass);
    return CONNECTING;

  case CONNECTING:
    switch (WiFi.status()) {
    case WL_NO_SHIELD:
      error("Connection to %s failed: there's no WiFi shield", creds_.ssid);
      return CONNECTION_FAILED;

    case WL_NO_SSID_AVAIL:
      error("Connection to %s failed: SSID not available", creds_.ssid);
      return CONNECTION_FAILED;

    case WL_SCAN_COMPLETED:
      debug("Scan completed, still connecting to %s...", creds_.ssid);
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
        error("Can't begin multicast on port %d, multicast group %s", port_,
              mcastAddr_);
        goto err;
      }
      IPAddress ip = WiFi.localIP();
      info("Connected to %s, IP: %d.%d.%d.%d, bound to port %d, multicast group: %s",
           creds_.ssid, ip[0], ip[1], ip[2], ip[3], port_, mcastAddr_);
      return CONNECTED;
    }
    break;

    case WL_CONNECT_FAILED:
      error("Connection to %s failed", creds_.ssid);
      return CONNECTION_FAILED;

    case WL_CONNECTION_LOST:
      error("Connection to %s lost", creds_.ssid);
      return INITIALIZING;

    case WL_DISCONNECTED:
    default: {
      static int32_t last_t = 0;
      if (timeMillis() - last_t > 5000) {
        int st = WiFi.status();
        if (st == WL_DISCONNECTED) {
          debug("Still connecting...");
        } else {
          info("Still connecting, unknown status code %d", st);
        }
        last_t = timeMillis();
      }
    }
    }
    break;

  case CONNECTED:
  case DISCONNECTED:
  case CONNECTION_FAILED:
    // do nothing
    break;

  case DISCONNECTING:
    switch (WiFi.status()) {
    case WL_DISCONNECTED:
      info("Disconnected from %s", creds_.ssid);
      return DISCONNECTED;
    default:
      info("Disconnecting from %s...", creds_.ssid);
      WiFi.disconnect();
      break;
    }
    break;
  }

  return status;

err:
  error("Connection to %s failed", creds_.ssid);
  WiFi.disconnect();
  return CONNECTION_FAILED;
}

int Esp8266WiFi::recv(void* buf, size_t bufsize) {
  int cb = udp_.parsePacket();
  if (cb <= 0) {
    return 0;
  }
  return udp_.read((unsigned char*)buf, bufsize);
}

#ifndef DISABLE_WIFI_SENDING
#  define DISABLE_WIFI_SENDING 0
#endif // DISABLE_WIFI_SENDING

void Esp8266WiFi::send(void* buf, size_t bufsize) {
#if !DISABLE_WIFI_SENDING
  IPAddress ip;
  ip.fromString(mcastAddr_);
  udp_.beginPacket(ip, port_);
  udp_.write(static_cast<uint8_t*>(buf), bufsize);
  udp_.endPacket();
#else // DISABLE_WIFI_SENDING
  (void)buf;
  (void)bufsize;
#endif // DISABLE_WIFI_SENDING
}


} // namespace unisparks

#endif // UNISPARKS_ESP8266WIFI
