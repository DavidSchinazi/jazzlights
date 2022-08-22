#ifndef UNISPARKS_NETWORKS_ESP8266_H
#define UNISPARKS_NETWORKS_ESP8266_H

#include "unisparks/network.hpp"

#ifndef UNISPARKS_ESP8266WIFI
#  if defined(ESP8266) || defined(ESP32)
#    define UNISPARKS_ESP8266WIFI 1
#  else // defined(ESP8266) || defined(ESP32)
#    define UNISPARKS_ESP8266WIFI 0
#  endif  // defined(ESP8266) || defined(ESP32)
#endif // UNISPARKS_ESP8266WIFI

#if UNISPARKS_ESP8266WIFI

#if defined(ESP8266)
#  include <ESP8266WiFi.h>
#elif defined(ESP32)
#  include <WiFi.h>
#endif

#include <WiFiUdp.h>

namespace unisparks {


class Esp8266WiFi : public UdpNetwork {
 public:
  Esp8266WiFi(const char* ssid, const char* pass);

  NetworkStatus update(NetworkStatus status, Milliseconds currentTime) override;
  int recv(void* buf, size_t bufsize, std::string* details) override;
  void send(void* buf, size_t bufsize) override;
  NetworkDeviceId getLocalDeviceId() override {
    return localDeviceId_;
  }
  const char* networkName() const override {
    return "ESP8266WiFi";
  }

  struct Credentials {
    const char* ssid;
    const char* pass;
  };

  struct StaticConf {
    const char* ip;
    const char* gateway;
    const char* subnetMask;
  };

  std::string statusStr(Milliseconds currentTime);

 private:
  static constexpr wl_status_t kUninitialized = static_cast<wl_status_t>(123);
  static std::string WiFiStatusToString(wl_status_t status);
  int port_ = DEFAULT_UDP_PORT;
  const char* mcastAddr_ = DEFAULT_MULTICAST_ADDR;
  WiFiUDP udp_;
  Credentials creds_;
  StaticConf* staticConf_ = nullptr;
  NetworkDeviceId localDeviceId_;
  wl_status_t currentWiFiStatus_ = kUninitialized;
  Milliseconds timeOfLastWiFiStatusTransition_ = -1;
  bool attemptingDhcp_ = true;
};

} // namespace unisparks

#endif // UNISPARKS_ESP8266WIFI
#endif // UNISPARKS_NETWORKS_ESP8266_H
