#include "dfsparks/networks/esp8266wifi.h"
#ifdef ESP8266
#include <WiFiUdp.h>
#include "dfsparks/log.h"

namespace dfsparks {

Esp8266Network::Esp8266Network(const char* ssid, const char *pass) {
  creds_ = {ssid, pass};
}

void Esp8266Network::doConnection() {
  enum Status {disconnected, connecting, connected, disconnecting, connection_failed};
  switch(status()) {
  case Network::connected:
  case Network::disconnected:
  case Network::connection_failed:
    // do nothing
    break;

  case Network::connecting:
    if (needToBegin_) {
        info("Connecting to %s...", creds_.ssid);
        WiFi.begin(creds_.ssid, creds_.pass);            
        needToBegin_ = false;     
    }
    switch(WiFi.status()) {
      case WL_CONNECTED: {
        IPAddress mcaddr;
        mcaddr.fromString(multicast_addr);
        int res = udp_.beginMulticast(WiFi.localIP(), mcaddr, udp_port);
        if (!res) {
          error("Can't begin multicast on port %d, multicast group %s", udp_port,
                multicast_addr);
          goto err;
        }
        IPAddress ip = WiFi.localIP();
        info("Connected, IP: %d.%d.%d.%d, bound to port %d, multicast group: %s", 
          ip[0], ip[1], ip[2], ip[3], udp_port, multicast_addr);
        setStatus(Network::connected);        
      }
      break;

      // case WL_DISCONNECTED: {
      //   goto err;
      // }
      // break;

      default: {
        static int32_t last_t = 0;
        if (timeMillis() - last_t > 500) {
          info("Still connecting... %d (CNTD: %d, DCNTD: %d, IDLE: %d)", WiFi.status(), WL_CONNECTED, WL_DISCONNECTED, WL_IDLE_STATUS);
          last_t = timeMillis();
        }         
      }
    }
    break;

  case Network::disconnecting:
    switch(WiFi.status()) {
      case WL_DISCONNECTED:
        info("Disconnected from %s", creds_.ssid);
        setStatus(Network::disconnected);    
        break;      
      case WL_CONNECTED:
        WiFi.disconnect();
        break;
    }
    break;
  }
  return;

err:
  error("Connection to %s failed", creds_.ssid);
  setStatus(Network::connection_failed);
  WiFi.disconnect();
}

int Esp8266Network::doReceive(void *buf, size_t bufsize) {
  int cb = udp_.parsePacket();
  if (cb <= 0) {
    return 0;
  }
  return udp_.read((unsigned char *)buf, bufsize);
}

int Esp8266Network::doBroadcast(void *buf, size_t bufsize) {
  (void)buf;
  (void)bufsize;
  // IPAddress ip(255, 255, 255, 255);
  // udp.beginPacket(ip, port);
  // udp.write(buf, bufsize);
  // udp.endPacket();
  return 0;
}

} // namespace dfsparks

#endif /* ESP8266 */
