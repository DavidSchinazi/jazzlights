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

static constexpr int msgcodeEffectFrame = 0xDF0002;
static constexpr Milliseconds effectUpdatePeriod = 200;
static constexpr Milliseconds maxMissingEffectMasterPeriod = 2000;

struct Message {
  int32_t msgcode;
  union {
    struct Frame {
      int32_t sequence;
      char reserved[8];
      char pattern[16];
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

void Network::disconnect() {
  if (status_ == CONNECTED) {
    status_ = update(DISCONNECTING);
  }
}

void Network::reconnect() {
  if (status_ != CONNECTED) {
    lastConnectionAttempt_ = timeMillis();
    status_ = update(CONNECTING);
  }
}

// void Network::broadcastEffect(const char *name, Milliseconds time) {
//     isEffectMaster_ = true;
//     strncpy(effectName_, name, sizeof(effectName_));
//     effectStartTime_ = timeMillis() - time;
//     effectLastTxTime_ = 0;
//     communicate();
// }

// void Network::broadcastTempo(BeatsPerMinute /*tempo*/) {
//   /* not implemented yet */
// }

bool Network::sync(const char** pattern, Milliseconds* time,
                   BeatsPerMinute* /*tempo*/) {
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
    isEffectMaster_ = false;
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
    info("Haven't heard from effect master for a while, taking over");
    isEffectMaster_ = true;
  }

  Message packet;

  // Do we need to broadcast?
  if (isEffectMaster_ && (effectLastTxTime_ < 1
                          || currTime - effectLastTxTime_ > effectUpdatePeriod)) {
    packet.msgcode = htonl(msgcodeEffectFrame);
    strncpy(packet.data.frame.pattern, *pattern,
            sizeof(packet.data.frame.pattern));
    packet.data.frame.time = htonl(*time);
    effectLastTxTime_ = currTime;
    send(&packet, sizeof(packet));
    debug("Sent pattern %s, time %d ms", *pattern, *time);
  }

  // Now let's see if we received anything
  size_t n = recv(&packet, sizeof(packet));
  if (n == 0) {
    return false;
  }
  if (n < sizeof(packet)) {
    error("Received packet too short, received %d bytes, expected at least %d bytes",
          n, sizeof(packet));
    return false;
  }
  int32_t msgcode = ntohl(packet.msgcode);
  if (msgcode == msgcodeEffectFrame) {
    effectLastRxTime_ = currTime;

    if (effectLastRxTime_ - effectLastTxTime_ < 50) {
      debug("This must be my own packet, ignoring");
      return false;
    }

    isEffectMaster_ = false;
    strncpy(patternBuf_, packet.data.frame.pattern, sizeof(patternBuf_));
    *time = static_cast<Milliseconds>(ntohl(packet.data.frame.time));
    *pattern = patternBuf_;
    debug("Received pattern %s, time %d ms", *pattern, *time);
    return true;
  } else {
    error("Received unknown message, code: %d", msgcode);
    return false;
  }
  return false;
}

bool Network::isControllingEffects() const {
  return isEffectMaster_;
}

Network& Network::isControllingEffects(bool v) {
  isEffectMaster_ = v;
  if (v) {
    effectLastTxTime_ = 0;
  }
  return *this;
}


struct DummyNetwork : public Network {
  NetworkStatus update(NetworkStatus status) override {
    switch (status) {
    case INITIALIZING:
    case CONNECTING:
      return CONNECTED;

    case DISCONNECTING:
      return DISCONNECTED;

    default:
      return status;
    }
  }

  int recv(void* /*buf*/, size_t /*bufsize*/) override {
    return 0;
  }

  void send(void* /*buf*/, size_t /*bufsize*/) override {
  }
};

Network& dummyNetwork() {
  static DummyNetwork inst;
  return inst;
}

} // namespace unisparks

