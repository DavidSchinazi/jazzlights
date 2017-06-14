#include "unisparks/networks/esp8266wifi.h"
#ifdef ARDUINO
#include "unisparks/networks/arduinoethernet.h"
#include "unisparks/log.h"
#include "unisparks/timer.h"

namespace unisparks {

ArduinoEthernetNetwork::ArduinoEthernetNetwork(MacAddress mac) : mac_(mac) {
}

void ArduinoEthernetNetwork::doConnection() {
  enum Status {disconnected, connecting, connected, disconnecting, connection_failed};
  switch(status()) {
  case Network::disconnected:
  case Network::connection_failed:
    // do nothing
    break;

  case Network::beginning:
    Ethernet.begin(mac_.bytes);
    udp_.begin(udp_port);            
    setStatus(Network::connected);
    break;

  case Network::connected:
    Ethernet.maintain();
    break;

  case Network::disconnecting:
    udp_.stop();
    setStatus(Network::disconnected);
    break;
  }
}

int ArduinoEthernetNetwork::doReceive(void *buf, size_t bufsize) {
  if (status() != Network::connected) {
    return 0;
  }

  int cb = udp_.parsePacket();
  if (cb <= 0) {
    return 0;
  }
  return udp_.read((unsigned char *)buf, bufsize);
}

int ArduinoEthernetNetwork::doBroadcast(void *buf, size_t bufsize) {
  if (status() != Network::connected) {
    return 0;
  }
  // We don't run ESP8266 devices in server mode yet
  // IPAddress ip(255, 255, 255, 255);
  // udp_.beginPacket(ip, udp_port);
  // udp_.write(buf, bufsize);
  // udp_.endPacket();
  return 0;
}

} // namespace unisparks

#endif /* ARDUINO */
