#include "unisparks/networks/esp8266wifi.h"
#ifdef ESP8266
#include <WiFiUdp.h>
#include "unisparks/log.h"
#include "unisparks/timer.h"

namespace unisparks {

Esp8266Network::Esp8266Network(const char* ssid, const char *pass) : creds_{ssid, pass} {
}

/*
WL_NO_SHIELD        = 255,   // for compatibility with WiFi Shield library
    WL_IDLE_STATUS      = 0,
    WL_NO_SSID_AVAIL    = 1,
    WL_SCAN_COMPLETED   = 2,
    WL_CONNECTED        = 3,
    WL_CONNECT_FAILED   = 4,
    WL_CONNECTION_LOST  = 5,
    WL_DISCONNECTED     = 6
    */

void Esp8266Network::doConnection() {
  switch(status()) {
  case Network::beginning:
    info("Connecting to %s...", creds_.ssid);
    WiFi.begin(creds_.ssid, creds_.pass);            
    setStatus(Network::connecting);
    break;

  case Network::connecting:
    switch(WiFi.status()) {
      case WL_NO_SHIELD: 
        error("Connection failed: there's no WiFi shield");
        setStatus(Network::connection_failed);
        break;

      case WL_NO_SSID_AVAIL: 
        error("Connection failed: SSID not available");
        setStatus(Network::connection_failed);
        break;

      case WL_SCAN_COMPLETED: 
        info("Scan completed, still connecting...");
        break;

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

      case WL_CONNECT_FAILED: 
        error("Connection failed");
        setStatus(Network::connection_failed);
        break;

      case WL_CONNECTION_LOST: 
        error("Connection lost");
        setStatus(Network::beginning);
        break;

      case WL_DISCONNECTED:
      default: {
        static int32_t last_t = 0;
        if (timeMillis() - last_t > 500) {
          int st = WiFi.status();
          if (st == WL_DISCONNECTED) {
            info("Still connecting...");
          }
          else {
            info("Still connecting, unknown status code %d", st);
          }
          last_t = timeMillis();
        }         
      }
    }
    break;

  case Network::connected:
  case Network::disconnected:
  case Network::connection_failed:
    // do nothing
    break;

  case Network::disconnecting:
    switch(WiFi.status()) {
      case WL_DISCONNECTED:
        info("Disconnected from %s", creds_.ssid);
        setStatus(Network::disconnected);    
        break;      
      default:
        info("Disconnecting...");
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
  // We don't run ESP8266 devices in server mode yet
  // IPAddress ip(255, 255, 255, 255);
  // udp.beginPacket(ip, port);
  // udp.write(buf, bufsize);
  // udp.endPacket();
  return 0;
}

} // namespace unisparks

#endif /* ESP8266 */
