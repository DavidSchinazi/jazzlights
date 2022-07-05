#include "unisparks/networks/arduinoethernet.hpp"

#include <sstream>

#include "unisparks/util/log.hpp"
#include "unisparks/util/time.hpp"

#if UNISPARKS_ARDUINO_ETHERNET

namespace unisparks {
namespace {

std::string EthernetLinkStatusToString(EthernetLinkStatus linkStatus) {
  switch (linkStatus) {
    case Unknown: return "Unknown";
    case LinkON: return "ON";
    case LinkOFF: return "OFF";
  }
  std::ostringstream s;
  s << "UNKNOWN(" << static_cast<int>(linkStatus) << ")";
  return s.str();
}

std::string EthernetHardwareStatusToString(EthernetHardwareStatus hwStatus) {
  switch (hwStatus) {
    case EthernetNoHardware: return "NoHardware";
    case EthernetW5100: return "W5100";
    case EthernetW5200: return "W5200";
    case EthernetW5500: return "W5500";
  }
  std::ostringstream s;
  s << "UNKNOWN(" << static_cast<int>(hwStatus) << ")";
  return s.str();
}

} // namespace

ArduinoEthernetNetwork::ArduinoEthernetNetwork(NetworkDeviceId localDeviceId) :
    localDeviceId_(localDeviceId) {
  // These pins work with the M5Stack ATOM Matrix (or ATOM Lite) connected to the ATOM PoE Kit.
  // https://shop.m5stack.com/products/atom-poe-kit-with-w5500-hy601742e
  SPI.begin(/*SCK=*/22, /*MISO=*/23, /*MOSI=*/33, /*SS=*/-1);
  Ethernet.init(/*=CS*/19);
}

NetworkStatus ArduinoEthernetNetwork::update(NetworkStatus status, Milliseconds currentTime) {
  switch (status) {
    case INITIALIZING: {
      EthernetHardwareStatus hwStatus = Ethernet.hardwareStatus();
      if (hwStatus == EthernetNoHardware) {
        error("%u %s Failed to communicate with Ethernet hardware", currentTime, name());
        return CONNECTION_FAILED;
      }
      info("%u %s Ethernet detected hardware status %s with MAC address " DEVICE_ID_FMT,
           currentTime, name(), EthernetHardwareStatusToString(hwStatus).c_str());
      return CONNECTING;
    }
    case CONNECTING: {
      EthernetLinkStatus linkStatus = Ethernet.linkStatus();
      if (linkStatus != LinkON) {
        error("%u %s Ethernet is not plugged in (state %s)",
              currentTime, name(), EthernetLinkStatusToString(linkStatus).c_str());
        return CONNECTION_FAILED;
      }
      constexpr unsigned long kDhcpTimeoutMs = 5000;
      constexpr unsigned long kResponseTimeoutMs = 1000;
      // TODO move Ethernet.begin() call to its own thread because it performs DHCP synchronously
      // and currently blocks our main thread while waiting for a DHCP response.
      int beginRes = Ethernet.begin(localDeviceId_.data(), kDhcpTimeoutMs, kResponseTimeoutMs);
      if (beginRes == 0) {
        error("%u %s Ethernet DHCP failed", currentTime, name());
        // TODO add support for IPv4 link-local addresses.
        return CONNECTION_FAILED;
      }
      IPAddress ip = Ethernet.localIP();
      info("%u %s Ethernet DHCP provided IP: %d.%d.%d.%d, bound to port %d, multicast group: %s",
          currentTime, name(), ip[0], ip[1], ip[2], ip[3], port_, mcastAddr_);
      IPAddress mcaddr;
      mcaddr.fromString(mcastAddr_);
      udp_.beginMulticast(mcaddr, port_);
      return CONNECTED;
    }
    case CONNECTED: {
      Ethernet.maintain();
      return status;
    }
    case DISCONNECTING: {
      udp_.stop();
      return DISCONNECTED;
    }
    case DISCONNECTED:
    case CONNECTION_FAILED:
      // Do nothing.
      return status;
  }
  return status;
}

int ArduinoEthernetNetwork::recv(void* buf, size_t bufsize, std::string* /*details*/) {
  int cb = udp_.parsePacket();
  if (cb <= 0) {
    return 0;
  }
  return udp_.read((unsigned char*)buf, bufsize);
}

void ArduinoEthernetNetwork::send(void* buf, size_t bufsize) {
  IPAddress ip;
  ip.fromString(mcastAddr_);
  udp_.beginPacket(ip, port_);
  udp_.write(static_cast<uint8_t*>(buf), bufsize);
  udp_.endPacket();
}

} // namespace unisparks

#endif // UNISPARKS_ARDUINO_ETHERNET
