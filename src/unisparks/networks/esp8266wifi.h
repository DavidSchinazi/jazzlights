#ifdef ESP8266
#ifndef UNISPARKS_NETWORK_ESP8266_H
#define UNISPARKS_NETWORK_ESP8266_H
#include "unisparks/network.h"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

namespace unisparks {

class Esp8266Network : public Network {
public:
  Esp8266Network(const char* ssid, const char* passwd);

private:
  void doConnection() final;
  int doReceive(void *buf, size_t bufsize) final;
  int doBroadcast(void *buf, size_t bufsize) final;

  struct Credentials {
    const char* ssid;
    const char* pass;
  };

  WiFiUDP udp_;
  Credentials creds_;
  bool needToBegin_ = true;
};

} // namespace unisparks

#endif /* UNISPARKS_NETWORK_ESP8266UDP_H */
#endif /* ESP8266 */
