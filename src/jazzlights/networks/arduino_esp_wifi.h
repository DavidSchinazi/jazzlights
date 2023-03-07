#ifndef JL_NETWORKS_ARDUINO_ESP_WIFI_H
#define JL_NETWORKS_ARDUINO_ESP_WIFI_H

#include "jazzlights/network.h"

#ifndef JL_ESP_WIFI
#if defined(ESP8266) || defined(ESP32)
#define JL_ESP_WIFI 1
#else  // defined(ESP8266) || defined(ESP32)
#define JL_ESP_WIFI 0
#endif  // defined(ESP8266) || defined(ESP32)
#endif  // JL_ESP_WIFI

#if JL_ESP_WIFI

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

#include <WiFiUdp.h>

namespace jazzlights {

class ArduinoEspWiFiNetwork : public UdpNetwork {
 public:
  static ArduinoEspWiFiNetwork* get();

  NetworkStatus update(NetworkStatus status, Milliseconds currentTime) override;
  int recv(void* buf, size_t bufsize, std::string* details) override;
  void send(void* buf, size_t bufsize) override;
  NetworkDeviceId getLocalDeviceId() override { return localDeviceId_; }
  const char* networkName() const override { return "ArduinoEspWiFiNetwork"; }
  std::string getStatusStr(Milliseconds currentTime) const override;

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
  ArduinoEspWiFiNetwork(const char* ssid, const char* pass);
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

}  // namespace jazzlights

#endif  // JL_ESP_WIFI
#endif  // JL_NETWORKS_ARDUINO_ESP_WIFI_H
