#ifndef UNISPARKS_NETWORKS_ESP8266_H
#define UNISPARKS_NETWORKS_ESP8266_H
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "unisparks/network.hpp"

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

#endif // ESP8266
#endif /* UNISPARKS_NETWORKS_ESP8266_H */
