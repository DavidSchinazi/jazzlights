#ifndef JL_NETWORKS_ARDUINO_ESP_WIFI_H
#define JL_NETWORKS_ARDUINO_ESP_WIFI_H

#ifdef ARDUINO

#include "jazzlights/network.h"

#ifndef JL_ESP_WIFI
#define JL_ESP_WIFI 1
#endif  // JL_ESP_WIFI

#if JL_ESP_WIFI

#include <WiFi.h>
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
  const char* shortNetworkName() const override { return "WiFi"; }
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
  uint16_t port_ = DefaultUdpPort();
  const char* mcastAddr_ = DefaultMulticastAddress();
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
#endif  // ARDUINO
#endif  // JL_NETWORKS_ARDUINO_ESP_WIFI_H
