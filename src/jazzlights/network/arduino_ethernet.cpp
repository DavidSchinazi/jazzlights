#include "jazzlights/network/arduino_ethernet.h"

#if JL_ETHERNET && !JL_ESP32_ETHERNET

#include <sstream>

#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

namespace jazzlights {
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

}  // namespace

// static
NetworkDeviceId ArduinoEthernetNetwork::QueryLocalDeviceId() {
  uint8_t wifiMacAddress[6] = {};
  uint64_t efuseMac64 = ESP.getEfuseMac();
  if (efuseMac64 == 0) { jll_fatal("%u Wi-Fi failed to read MAC address from EFUSE", timeMillis()); }
  memcpy(wifiMacAddress, &efuseMac64, sizeof(wifiMacAddress));
  return NetworkDeviceId(wifiMacAddress).PlusOne();
}

ArduinoEthernetNetwork::ArduinoEthernetNetwork() {
  jll_info("%u Starting Ethernet with MAC address " DEVICE_ID_FMT, timeMillis(), DEVICE_ID_HEX(localDeviceId_));
#if JL_CORE2AWS_ETHERNET
  // These pins work with the M5Stack Core2AWS connected to the Ethernet W5500 module.
  // https://docs.m5stack.com/en/core/core2_for_aws
  // https://docs.m5stack.com/en/base/lan_v12
  // Note that the pinout docs for the ethernet Core2 base are wrong:
  // bus position | Core2 pin | W5500 module pin
  //            9 |        38 | 19 = W5500 MISO
  //           15 |        13 | 16
  //           16 |        14 | 17
  //           19 |        32 |  2
  //           20 |        33 |  5
  //           21 |        27 | 12
  //           22 |        19 | 13 = W5500 RST
  //           23 |         2 | 15
  // For that reason we need to set MISO to 38 and not 19.
  // Luckily 38 matches the MISO for the Core2 LCD and TF card reader.
  SPI.begin(/*SCLK=*/18, /*MISO=*/38, /*MOSI=*/23, /*SS/CS=*/-1);
  Ethernet.init(/*SS/CS=*/26);
#else   // JL_CORE2AWS_ETHERNET
  // These pins work with the M5Stack ATOM Matrix (or ATOM Lite) connected to the ATOM PoE Kit.
  // https://shop.m5stack.com/products/atom-poe-kit-with-w5500-hy601742e
  SPI.begin(/*SCLK=*/22, /*MISO=*/23, /*MOSI=*/33, /*SS/CS=*/-1);
  Ethernet.init(/*SS/CS=*/19);
#endif  // JL_CORE2AWS_ETHERNET
}

// static
ArduinoEthernetNetwork* ArduinoEthernetNetwork::get() {
  static ArduinoEthernetNetwork sSingleton;
  return &sSingleton;
}

NetworkStatus ArduinoEthernetNetwork::update(NetworkStatus status, Milliseconds currentTime) {
  switch (status) {
    case INITIALIZING: {
      EthernetHardwareStatus hwStatus = Ethernet.hardwareStatus();
      if (hwStatus == EthernetNoHardware) {
        jll_error("%u Ethernet failed to communicate with hardware", currentTime);
        // In some cases this will fail because the W5500 chip hasn't booted yet.
        // Currently, this is fixed by the automatic reconnect attempt 10s later.
        // TODO try much sooner to avoid waiting 10s.
        return CONNECTION_FAILED;
      }
      jll_info("%u Ethernet detected hardware status %s with MAC address " DEVICE_ID_FMT, currentTime,
               EthernetHardwareStatusToString(hwStatus).c_str(), DEVICE_ID_HEX(localDeviceId_));
      return CONNECTING;
    }
    case CONNECTING: {
      EthernetLinkStatus linkStatus = Ethernet.linkStatus();
      if (linkStatus != LinkON) {
        jll_error("%u Ethernet is not plugged in (state %s)", currentTime,
                  EthernetLinkStatusToString(linkStatus).c_str());
        return CONNECTION_FAILED;
      }
      constexpr unsigned long kDhcpTimeoutMs = 5000;
      constexpr unsigned long kResponseTimeoutMs = 1000;
      // TODO move Ethernet.begin() call to its own thread because it performs DHCP synchronously
      // and currently blocks our main thread while waiting for a DHCP response.
      uint8_t macAddress[6];
      localDeviceId_.writeTo(macAddress);
      int beginRes = Ethernet.begin(macAddress, kDhcpTimeoutMs, kResponseTimeoutMs);
      if (beginRes == 0) {
        jll_error("%u Ethernet DHCP failed", currentTime);
        // TODO add support for IPv4 link-local addresses.
        return CONNECTION_FAILED;
      }
      IPAddress ip = Ethernet.localIP();
      jll_info("%u Ethernet DHCP provided IP: %d.%d.%d.%d, bound to port %d, multicast group: %s", currentTime, ip[0],
               ip[1], ip[2], ip[3], port_, mcastAddr_);
      IPAddress mcaddr;
      mcaddr.fromString(mcastAddr_);
      udp_.beginMulticast(mcaddr, port_);
      return CONNECTED;
    }
    case CONNECTED: {
      Ethernet.maintain();
      return status;
    }
    case CONNECTION_FAILED:
      // Do nothing.
      return status;
  }
  return status;
}

std::string ArduinoEthernetNetwork::getStatusStr(Milliseconds currentTime) {
  switch (getStatus()) {
    case INITIALIZING: return "init";
    case CONNECTING: return "connecting";
    case CONNECTION_FAILED: return "failed";
    case CONNECTED: {
      IPAddress ip = Ethernet.localIP();
      const Milliseconds lastRcv = getLastReceiveTime();
      char statStr[100] = {};
      snprintf(statStr, sizeof(statStr) - 1, "%u.%u.%u.%u - %ums", ip[0], ip[1], ip[2], ip[3],
               (lastRcv >= 0 ? currentTime - lastRcv : -1));
      return std::string(statStr);
    }
  }
  return "error";
}

int ArduinoEthernetNetwork::recv(void* buf, size_t bufsize, std::string* /*details*/) {
  // TODO: figure out why udp_.parsePacket() sometimes blocks for multiple seconds or indefinitely.
  // From observing logs it looks like it sometimes returns way more than what would be expected in a single packet even
  // though it's supposed to return how many bytes are available in the next packet. From looking at the source code for
  // EthernetUDP::parsePacket() there's a while loop with a comment about infinite looping which is incredibly
  // suspicious.
  int cb = udp_.parsePacket();
  if (cb <= 0) { return 0; }
  return udp_.read((unsigned char*)buf, bufsize);
}

void ArduinoEthernetNetwork::send(void* buf, size_t bufsize) {
  IPAddress ip;
  ip.fromString(mcastAddr_);
  udp_.beginPacket(ip, port_);
  udp_.write(static_cast<uint8_t*>(buf), bufsize);
  udp_.endPacket();
}

}  // namespace jazzlights

#endif  // JL_ETHERNET && !JL_ESP32_ETHERNET
