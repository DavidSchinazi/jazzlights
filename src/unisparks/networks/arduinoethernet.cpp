#include "unisparks/networks/arduinoethernet.hpp"
#include "unisparks/util/log.hpp"
#include "unisparks/util/time.hpp"

#ifdef ESP8266

namespace unisparks {

struct ArduinoEthernetNetwork : public UdpNetwork {
  NetworkStatus update(NetworkStatus status, Milliseconds currentTime) override;
  int recv(void* buf, size_t bufsize) override;
  void send(void* buf, size_t bufsize) override;
  const char* name() const override {
    return "ArduinoEthernet";
  }

  int port_;
  MacAddress mac_;
  EthernetUDP udp_;
};

NetworkStatus ArduinoEthernetNetwork::update(NetworkStatus status, Milliseconds /*currentTime*/) {
  switch (status) {
  case INITIALIZING:
    Ethernet.begin(mac_.bytes);
    udp_.begin(port_);
    return CONNECTED;

  case CONNECTED:
    Ethernet.maintain();
    return status;

  case DISCONNECTING:
    udp_.stop();
    return DISCONNECTED;

  case CONNECTING:
  case DISCONNECTED:
  case CONNECTION_FAILED:
    // do nothing
    return status;
  }
  return status;
}

int ArduinoEthernetNetwork::recv(void* buf, size_t bufsize) {
  int cb = udp_.parsePacket();
  if (cb <= 0) {
    return 0;
  }
  return udp_.read((unsigned char*)buf, bufsize);
}

void ArduinoEthernetNetwork::send(void* buf, size_t bufsize) {
  // We don't run Arduino devices in server mode yet
  // IPAddress ip(255, 255, 255, 255);
  // udp_.beginPacket(ip, udp_port);
  // udp_.write(buf, bufsize);
  // udp_.endPacket();
}

} // namespace unisparks

#endif // ESP8266
