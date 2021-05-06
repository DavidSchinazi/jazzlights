#ifndef UNISPARKS_NETWORKS_ESP8266_H
#define UNISPARKS_NETWORKS_ESP8266_H

#include "unisparks/network.hpp"

#ifndef UNISPARKS_ESP8266WIFI
#  define UNISPARKS_ESP8266WIFI (defined(ESP8266) || defined(ESP32))
#endif // UNISPARKS_ESP8266WIFI

#if UNISPARKS_ESP8266WIFI

#if defined(ESP8266)
#  include <ESP8266WiFi.h>
#elif defined(ESP32)
#  include <WiFi.h>
#endif

#include <WiFiUdp.h>

namespace unisparks {


class Esp8266WiFi : public Network {
 public:
  Esp8266WiFi(const char* ssid, const char* pass) : creds_{ssid, pass} {
  }

  NetworkStatus update(NetworkStatus st) override;
  int recv(void* buf, size_t bufsize) override;
  void send(void* buf, size_t bufsize) override;

  struct Credentials {
    const char* ssid;
    const char* pass;
  };

  struct StaticConf {
    const char* ip;
    const char* gateway;
    const char* subnetMask;
  };

 private:
  int port_ = DEFAULT_UDP_PORT;
  const char* mcastAddr_ = DEFAULT_MULTICAST_ADDR;
  WiFiUDP udp_;
  Credentials creds_;
  StaticConf* staticConf_ = nullptr;
};

} // namespace unisparks

#endif // UNISPARKS_ESP8266WIFI
#endif // UNISPARKS_NETWORKS_ESP8266_H
