#include "renderers/pixelpusher.hpp"

#include <vector>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <list>

#include "unisparks/util/log.hpp"
#include "unisparks/util/time.hpp"

#ifndef REDUCE_PIXELPUSHER_BRIGHTNESS
#define REDUCE_PIXELPUSHER_BRIGHTNESS 0
#endif  // REDUCE_PIXELPUSHER_BRIGHTNESS

namespace unisparks {

#if REDUCE_PIXELPUSHER_BRIGHTNESS
uint8_t reduceOneChannelBrightness(uint8_t c) {
  uint16_t c16 = static_cast<uint16_t>(c);
  c16++;
  c16 *= 7;
  c16 /= 8;
  return static_cast<uint8_t>(c16);
}
#endif  // REDUCE_PIXELPUSHER_BRIGHTNESS

struct PixelPusherDiscoveryResult {
  struct sockaddr_in addressAndPort = {};
  int32_t controller = 0;
  int32_t group = 0;
  Milliseconds lastSeenTime = -1;
  PixelPusherDiscoveryResult() {
    addressAndPort.sin_family = AF_INET;
#ifdef __APPLE__
    addressAndPort.sin_len = sizeof(addressAndPort);
#endif // __APPLE__
  }
};

bool discoveryResultsMatch(const PixelPusherDiscoveryResult& a,
                           const PixelPusherDiscoveryResult& b) {
  return memcmp(&a.addressAndPort.sin_addr,
                &b.addressAndPort.sin_addr,
                sizeof(a.addressAndPort.sin_addr)) == 0 &&
         a.addressAndPort.sin_port == b.addressAndPort.sin_port &&
         a.controller == b.controller &&
         a.group == b.group;
}

class PixelPusherDiscovery {
 public:
  PixelPusherDiscovery() {
    fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) {
      fatal("Failed to create PixelPusherDiscovery socket: %s", strerror(errno));
    }
    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags == -1) {
      fatal("Failed to fcntl F_GETFL PixelPusherDiscovery socket: %s", strerror(errno));
    }
    if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
      fatal("Failed to fcntl F_SETFL PixelPusherDiscovery socket: %s", strerror(errno));
    }
    struct sockaddr_in localAddr = {};
#ifdef __APPLE__
    localAddr.sin_len = sizeof(localAddr);
#endif // __APPLE__
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(7331);
    if (bind(fd_, reinterpret_cast<struct sockaddr*>(&localAddr), sizeof(localAddr)) < 0) {
      fatal("Failed to bind PixelPusherDiscovery socket: %s", strerror(errno));
    }
    info("created PixelPusherDiscovery socket %d", fd_);
  }
  std::list<const struct sockaddr_in*> getAddresses(
      struct sockaddr_in* defaultAddressAndPort,
      int32_t controller, int32_t group, Milliseconds currentTime) {
    receiveLoop(currentTime);
    std::list<const struct sockaddr_in*> results;
    results.push_back(defaultAddressAndPort);
    if (controller != 0 && group != 0) {
      for (const PixelPusherDiscoveryResult& r : results_) {
        if (r.controller != controller || r.group != group) {
          continue;
        }
        bool alreadyInList = false;
        for (const struct sockaddr_in* sin : results) {
          if (memcmp(&r.addressAndPort.sin_addr,
                    &sin->sin_addr,
                    sizeof(sin->sin_addr)) == 0 &&
              r.addressAndPort.sin_port == sin->sin_port) {
            alreadyInList = true;
            break;;
          }
        }
        if (alreadyInList) {
          continue;
        }
        results.push_back(&r.addressAndPort);
      }
    }
    return results;
  }
 private:
  static uint16_t readUint16LE(const uint8_t* data) {
    return data[0] | (data[1] << 8);
  }
  static uint32_t readUint32LE(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
  }
  void receiveLoop(Milliseconds currentTime) {
    pruneResults(currentTime);
    while (true) {
      uint8_t buffer[2000] = {};
      struct sockaddr_in srcAddr = {};
      socklen_t srcAddrLen = sizeof(srcAddr);
      ssize_t recvLen = recvfrom(fd_, buffer, sizeof(buffer), /*flags=*/0,
                                 reinterpret_cast<struct sockaddr*>(&srcAddr), &srcAddrLen);
      if (recvLen < 0) {
        if (errno != EAGAIN) {
          error("PixelPusherDiscovery recvfrom failed: %s", strerror(errno));
        }
        break;
      }
      static constexpr ssize_t kMinPacketLength =
        6 + 4 + 1 + 1 + 2 + 2 + 2 + 2 + 4 + 1 + 1 + 2 + 4 + 4 + 4 + 4 + 4 + 2 + 2 + 2;
      if (recvLen < kMinPacketLength) {
        error("PixelPusherDiscovery received unexpected length %zu < %zu", recvLen, kMinPacketLength);
        continue;
      }
      if (buffer[10] != 2) {
        error("PixelPusherDiscovery received non-PixelPusher type %u", buffer[10]);
        continue;
      }
      static constexpr size_t kIpAddressOffset = 6;
      static constexpr size_t kPortOffset =
        6 + 4 + 1 + 1 + 2 + 2 + 2 + 2 + 4 + 1 + 1 + 2 + 4 + 4 + 4 + 4 + 4 + 2 + 2;
      static constexpr size_t kControllerOffset =
        6 + 4 + 1 + 1 + 2 + 2 + 2 + 2 + 4 + 1 + 1 + 2 + 4 + 4 + 4;
      static constexpr size_t kGroupOffset =
        6 + 4 + 1 + 1 + 2 + 2 + 2 + 2 + 4 + 1 + 1 + 2 + 4 + 4 + 4 + 4;
      char srcAddrStr[INET_ADDRSTRLEN] = {};
      inet_ntop(AF_INET, &(srcAddr.sin_addr), srcAddrStr, sizeof(srcAddrStr));
      debug("PixelPusherDiscovery: %02x:%02x:%02x:%02x:%02x:%02x %u.%u.%u.%u:%u c%u g%u from %s:%u",
            buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5],
            buffer[6], buffer[7], buffer[8], buffer[9],
            readUint16LE(&buffer[kPortOffset]),
            readUint32LE(&buffer[kControllerOffset]),
            readUint32LE(&buffer[kGroupOffset]),
            srcAddrStr, ntohs(srcAddr.sin_port));
      {
        // Save first result from IP they think they have.
        PixelPusherDiscoveryResult result;
        memcpy(&result.addressAndPort.sin_addr,
              &buffer[kIpAddressOffset],
              sizeof(result.addressAndPort.sin_addr));
        result.addressAndPort.sin_port = htons(readUint16LE(&buffer[kPortOffset]));
        result.controller = readUint32LE(&buffer[kControllerOffset]);
        result.group = readUint32LE(&buffer[kGroupOffset]);
        result.lastSeenTime = currentTime;
        saveResult(result);
      }{
        // Save another result from the IP we actually heard from.
        PixelPusherDiscoveryResult result2;
        memcpy(&result2.addressAndPort.sin_addr,
              &(srcAddr.sin_addr),
              sizeof(result2.addressAndPort.sin_addr));
        result2.addressAndPort.sin_port = htons(readUint16LE(&buffer[kPortOffset]));
        result2.controller = readUint32LE(&buffer[kControllerOffset]);
        result2.group = readUint32LE(&buffer[kGroupOffset]);
        result2.lastSeenTime = currentTime;
        saveResult(result2);
      }
    }
  }
  void pruneResults(Milliseconds currentTime) {
    static constexpr Milliseconds kMaxLifeTime = 100 * 1000; // 100s.
    if (currentTime <= kMaxLifeTime) {
      return;
    }
    const Milliseconds oldestTime = currentTime - kMaxLifeTime;
    auto it = results_.begin();
    while (it != results_.end()) {
      if (it->lastSeenTime < oldestTime) {
        it = results_.erase(it);
      } else {
        ++it;
      }
    }
  }
  void saveResult(const PixelPusherDiscoveryResult& result) {
    for (PixelPusherDiscoveryResult& r : results_) {
      if (discoveryResultsMatch(r, result)) {
        r.lastSeenTime = result.lastSeenTime;
        return;
      }
    }
    results_.push_back(result);
  }
  std::list<PixelPusherDiscoveryResult> results_;
  int fd_ = -1;
};

static PixelPusherDiscovery gPixelPusherDiscovery;

static constexpr int HACKY_HARD_CODED_NUM_OF_LEDS=400;

PixelPusher::PixelPusher(const char* h, int p, int s, int t, int32_t controller, int32_t group) :
  host(h), port(p), strip(s), throttle(t), controller_(controller), group_(group), fd(0) {
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    fatal("can't create client socket: %s", strerror(errno));
  }

  struct hostent* hp;
  hp = gethostbyname(host);
  if (!hp) {
    fatal("can't connect to %s: %s", host, strerror(errno));
  }

  bzero((char*) &addr, sizeof(addr));
  addr.sin_family = AF_INET;
  bcopy((char*)hp->h_addr,
        (char*)&addr.sin_addr.s_addr, hp->h_length);
  addr.sin_port = htons(port);
}

void PixelPusher::render(InputStream<Color>& pixelColors) {
  assert(fd);

  Milliseconds currTime = timeMillis();
  if (lastTxTime > 0 && currTime - lastTxTime < throttle) {
    return;
  }
  lastTxTime = currTime;

  static uint32_t sequence = 1000000;

  std::vector<uint8_t> buf;
  buf.push_back((sequence >> 24) & 0xFF);
  buf.push_back((sequence >> 16) & 0xFF);
  buf.push_back((sequence >> 8) & 0xFF);
  buf.push_back(sequence & 0xFF);
  buf.push_back(strip & 0xFF);
  sequence++;

  int pxcnt = 0;
  for (auto color : pixelColors) {
    auto rgb = color.asRgb();
#if REDUCE_PIXELPUSHER_BRIGHTNESS
    buf.push_back(reduceOneChannelBrightness(rgb.red));
    buf.push_back(reduceOneChannelBrightness(rgb.green));
    buf.push_back(reduceOneChannelBrightness(rgb.blue));
#else  // REDUCE_PIXELPUSHER_BRIGHTNESS
    buf.push_back(rgb.red);
    buf.push_back(rgb.green);
    buf.push_back(rgb.blue);
#endif  // REDUCE_PIXELPUSHER_BRIGHTNESS
    pxcnt++;
    if (pxcnt > HACKY_HARD_CODED_NUM_OF_LEDS) {
      break;
    }
  }
  while (pxcnt < HACKY_HARD_CODED_NUM_OF_LEDS) {
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    pxcnt++;
  }

  for (const struct sockaddr_in* targetAddress :
      gPixelPusherDiscovery.getAddresses(&addr, controller_, group_, currTime)) {
    char ipAddressStr[INET_ADDRSTRLEN + 1] = {};
    inet_ntop(AF_INET, &(targetAddress->sin_addr), ipAddressStr, sizeof(ipAddressStr));
    if (sendto(fd, buf.data(), buf.size(), 0, reinterpret_cast<const struct sockaddr*>(targetAddress),
              sizeof(*targetAddress)) != static_cast<ssize_t>(buf.size())) {
      error("Can't send %d bytes to PixelPusher at %s:%u on socket %d: %s",
            buf.size(), ipAddressStr, ntohs(targetAddress->sin_port), fd, strerror(errno));
    } else {
      debug("Sent strip %d (%d pixels, %d bytes) to PixelPusher at %s:%u on socket %d",
            strip, pxcnt, buf.size(), ipAddressStr, ntohs(targetAddress->sin_port), fd);
    }
  }
}

// Each pixelpusher will broadcast these messages once per second on UDP port 7331.
// All integers are little endian.
// Those packets start with a DeviceHeader:
  /**
   * Device Header format:
   * uint8_t mac_address[6];
   * uint8_t ip_address[4];
   * uint8_t device_type;
   * uint8_t protocol_version; // for the device, not the discovery
   * uint16_t vendor_id;
   * uint16_t product_id;
   * uint16_t hw_revision;
   * uint16_t sw_revision;
   * uint32_t link_speed; // in bits per second
   */
// followed by a PixelPusher message when DeviceHeader.device_type = 2
  /**
   * uint8_t strips_attached;
   * uint8_t max_strips_per_packet;
   * uint16_t pixels_per_strip; // uint16_t used to make alignment work
   * uint32_t update_period; // in microseconds
   * uint32_t power_total; // in PWM units
   * uint32_t delta_sequence; // difference between received and expected sequence numbers
   * int32_t controller_ordinal;  // configured order number for controller
   * int32_t group_ordinal;  // configured group number for this controller
   * int16_t artnet_universe;
   * int16_t artnet_channel;
   * int16_t my_port;
   */
// group_ordinal/controller_ordinal is controlled by group/controller in pixel.rc

/* sudo tcpdump udp port 7331 -nvvX
09:30:56.387459 IP (tos 0x0, ttl 255, id 6, offset 0, flags [none], proto UDP (17), length 112)
    10.1.64.102.49153 > 255.255.255.255.7331: [udp sum ok] UDP, length 84
	0x0000:  ffff ffff ffff d880 3965 f0c0 0800 4500  ........9e....E.
	0x0010:  0070 0006 0000 ff11 7110 0a01 4066 ffff  .p......q...@f..
	0x0020:  ffff c001 1ca3 005c 891a d880 3965 f0c0  .......\....9e..
	0x0030:  0a01 4066 0201 0200 0100 0300 8f00 00e1  ..@f............
	0x0040:  f505 0801 9001 a086 0100 8073 0c00 0000  ...........s....
	0x0050:  0000 0000 0000 0000 0000 0000 0000 a31c  ................
	0x0060:  0000 0000 0000 0000 0000 0000 0c00 0000  ................
	0x0070:  0000 0000 0000 0000 0000 0000 0000       ..............

	0x0000:  ffff ffff ffff                           MAC destination = ff:ff:ff:ff:ff:ff
	0x0000:                 d880 3965 f0c0            MAC source      = d8:80:39:65:f0:c0
	0x0000:                                0800       MAC Ethertype/length
	0x0000:                                     4     IP version = 4
	0x0000:                                      5    IP header version length = 20
	0x0000:                                       00  IP ToS/DSCP+ECN
	0x0010:  0070                                     IP total length = 112
	0x0010:       0006                                IP fragment ID = 6
	0x0010:            0000                           IP flags and fragment offset = 0
	0x0010:                 ff                        IP TTL = 255
	0x0010:                   11                      IP proto = 17/UDP
	0x0010:                      7110                 IP header checksum
	0x0010:                           0a01 4066       source IP = 10.1.64.102
	0x0010:                                     ffff  destination IP = 255.255.255.255
	0x0020:  ffff                                     destination IP continued
	0x0020:       c001                                UDP source port = 49153
	0x0020:            1ca3                           UDP destination port = 7331
	0x0020:                 005c                      UDP total length = 92 (UDP payload length = 84)
	0x0020:                      891a                 UDP checksum
	0x0020:                           d880 3965 f0c0  DeviceHeader.mac_address = d8:80:39:65:f0:c0
	0x0030:  0a01 4066                                DeviceHeader.ip_address = 10.1.64.102
	0x0030:            02                             DeviceHeader.device_type = 2/PixelPusher
	0x0030:              01                           DeviceHeader.protocol_version = 1
	0x0030:                 0200                      DeviceHeader.vendor_id = 2
	0x0030:                      0100                 DeviceHeader.product_id = 1
	0x0030:                           0300            DeviceHeader.hw_revision = 3
	0x0030:                                8f00       DeviceHeader.sw_revision = 143
	0x0030:                                     00e1  DeviceHeader.link_speed = 100000000bps (100Mbps)
	0x0040:  f505                                     DeviceHeader.link_speed continued
	0x0040:       08                                  PixelPusher.strips_attached = 8
	0x0040:         01                                PixelPusher.max_strips_per_packet = 1
	0x0040:            9001                           PixelPusher.pixels_per_strip = 400
	0x0040:                 a086 0100                 PixelPusher.update_period = 100000ms (100s)
	0x0040:                           8073 0c00       PixelPusher.power_total = 816000
	0x0040:                                     0000  PixelPusher.delta_sequence = 0
	0x0050:  0000                                     PixelPusher.delta_sequence continued
	0x0050:       0000 0000                           PixelPusher.controller_ordinal = 0
	0x0050:                 0000 0000                 PixelPusher.group_ordinal = 0
	0x0050:                           0000            PixelPusher.artnet_universe = 0
	0x0050:                                0000       PixelPusher.artnet_channel = 0
	0x0050:                                     a31c  PixelPusher.my_port = 7331
	0x0060:  0000 0000 0000 0000 0000 0000 0c00 0000
	0x0070:  0000 0000 0000 0000 0000 0000 0000       30 unexpected bytes
*/

// 7331 = 0x a31c or 1ca3
// 5078 = 0x d613 or 13d6

// https://sites.google.com/a/heroicrobot.com/pixelpusher/pixelpusher-documentation/pixelpusher-hardware-configuration-guide
// https://sites.google.com/a/heroicrobot.com/pixelpusher/pixelpusher-documentation/pixelpusher-hardware-configuration-guide/software-getting-started-guide?authuser=0
// https://github.com/robot-head/PixelPusher-java/blob/master/src/com/heroicrobot/dropbit/registry/DeviceRegistry.java
// https://github.com/robot-head/PixelPusher-java/blob/master/src/com/heroicrobot/dropbit/discovery/DeviceHeader.java
// https://github.com/robot-head/PixelPusher-java/blob/master/src/com/heroicrobot/dropbit/discovery/DeviceType.java
// https://github.com/robot-head/PixelPusher-java/blob/master/src/com/heroicrobot/dropbit/devices/pixelpusher/PixelPusher.java

} // namespace unisparks
