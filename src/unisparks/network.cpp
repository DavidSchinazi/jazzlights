#include "unisparks/network.hpp"

#include "unisparks/util/log.hpp"
#include "unisparks/util/time.hpp"
#include "unisparks/util/math.hpp"

#include <stdint.h>
#include <string.h>
#ifndef ARDUINO
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#else
constexpr uint32_t ntohl(uint32_t n) {
  return ((n & 0xFF) << 24) | ((n & 0xFF00) << 8) | ((n & 0xFF0000) >> 8) |
         ((n & 0xFF000000) >> 24);
}

constexpr uint32_t htonl(uint32_t n) {
  return ((n & 0xFF) << 24) | ((n & 0xFF00) << 8) | ((n & 0xFF0000) >> 8) |
         ((n & 0xFF000000) >> 24);
}
#endif

namespace unisparks {

std::string displayBitsAsBinary(PatternBits p) {
  static_assert(sizeof(p) == 4, "32bits");
  char bits[33] = {};
  for (int b = 0; b < 32; b++) {
    if ((p >> b) & 1) {
      bits[b] = '.';
    } else {
      bits[b] = '_';
    }
  }
  return std::string(bits);
}

static constexpr int msgcodeEffectFrame = 0xDF0002;
static constexpr Milliseconds effectUpdatePeriod = 1000;
static constexpr Milliseconds maxMissingEffectMasterPeriod = 2000;

struct Message {
  int32_t msgcode;
  union {
    struct Frame {
      int32_t sequence;
      char reserved[8];
      PatternBits pattern;
      int32_t time;
      int32_t reserved1;
      uint16_t reserved2;
    } __attribute__((__packed__)) frame;
    // struct Beat {
    //   int32_t sequence;
    //   int16_t tempo;
    // } __attribute__((__packed__)) beat;
  } __attribute__((__packed__)) data;
} __attribute__((__packed__));



NetworkStatus Network::status() const {
  return status_;
}

void Network::reconnect() {
  if (status_ != CONNECTED) {
    lastConnectionAttempt_ = timeMillis();
    status_ = update(CONNECTING);
  }
}

bool Network::sync(PatternBits* pattern, Milliseconds* time) {
  Milliseconds currTime = timeMillis();
  if (status_ == CONNECTION_FAILED) {
    backoffTimeout_ = min(maxBackoffTimeout_, backoffTimeout_ * 2);
    if (currTime - lastConnectionAttempt_ > backoffTimeout_) {
      reconnect();
    }
  } else {
    status_ = update(status_);
  }
  if (status_ != CONNECTED) {
    if (isEffectMaster_) {
      info("%u No longer effect master because of network disconnection",
           currTime);
      isEffectMaster_ = false;
    }
    effectLastRxTime_ = 0;
    return false;
  }

  // We're connected
  backoffTimeout_ = minBackoffTimeout_;

  if (effectLastRxTime_ < 1) {
    effectLastRxTime_ = currTime;
  }

  if (!isEffectMaster_
      && currTime - effectLastRxTime_ > maxMissingEffectMasterPeriod) {
    info("%u Haven't heard from effect master for a while, taking over",
         currTime);
    isEffectMaster_ = true;
  }

  Message packet;

  // Do we need to broadcast?
  if (isEffectMaster_ && (effectLastTxTime_ < 1
                          || currTime - effectLastTxTime_ > effectUpdatePeriod
                          || *pattern != lastSentPattern_)) {
    memset(&packet, 0, sizeof(packet));
    packet.data.frame.reserved[6] = 0x33;
    packet.data.frame.reserved[7] = 0x77;
    packet.msgcode = htonl(msgcodeEffectFrame);
    packet.data.frame.pattern = *pattern;
    lastSentPattern_ = *pattern;
    packet.data.frame.time = htonl(*time);
    effectLastTxTime_ = currTime;
    send(&packet, sizeof(packet));
    info("%u Sent pattern %s, time %d ms",
         currTime, displayBitsAsBinary(*pattern).c_str(), *time);
  }

  // Now let's see if we received anything
  int n = recv(&packet, sizeof(packet));
  if (n <= 0) {
    return false;
  }
  if (n < static_cast<int>(sizeof(packet))) {
    error("%u Received packet too short, received %d bytes, expected at least %d bytes",
          currTime, n, sizeof(packet));
    return false;
  }
  int32_t msgcode = ntohl(packet.msgcode);
  if (msgcode == msgcodeEffectFrame) {
    if (packet.data.frame.reserved[6] != 0x33 ||
        packet.data.frame.reserved[7] != 0x77) {
      error("%u Ignoring message with bad reserved", currTime);
      return false;
    }
    effectLastRxTime_ = currTime;

    if (effectLastRxTime_ - effectLastTxTime_ < 50) {
      debug("%u This must be my own packet, ignoring", currTime);
      return false;
    }

    if (isEffectMaster_) {
      info("%u No longer effect master because of received packet", currTime);
    }
    isEffectMaster_ = false;
    pattern_ = packet.data.frame.pattern;
    *time = static_cast<Milliseconds>(ntohl(packet.data.frame.time));
    *pattern = pattern_;
    info("%u Received pattern %s, time %d ms",
         currTime, displayBitsAsBinary(*pattern).c_str(), *time);
    return true;
  } else {
    error("%u Received unknown message, code: %d", currTime, msgcode);
    return false;
  }
  return false;
}

void Network::triggerSendAsap(Milliseconds currentTime) {
  if (!isEffectMaster_) {
    info("%u Now controlling effects", currentTime);
  }
  isEffectMaster_ = true;
  effectLastTxTime_ = 0;
}

} // namespace unisparks
