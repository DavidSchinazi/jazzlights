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

std::string NetworkStatusToString(NetworkStatus status) {
  switch (status) {
#define X(s) case s: return #s;
    ALL_NETWORK_STATUSES
#undef X
  }
  return "UNKNOWN";
}

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

void Network::reconnect(Milliseconds currentTime) {
  if (status_ != CONNECTED) {
    lastConnectionAttempt_ = currentTime;
    info("%u Network Reconnecting", currentTime);
    status_ = update(CONNECTING);
  }
}

void UdpNetwork::triggerSendAsap(Milliseconds currentTime) {
  if (!isEffectMaster_) {
    info("%u Now controlling effects", currentTime);
  }
  isEffectMaster_ = true;
  effectLastTxTime_ = 0;
}

void UdpNetwork::setMessageToSend(const NetworkMessage& messageToSend,
                                  Milliseconds currentTime) {
  messageToSend_ = messageToSend;
}

std::list<NetworkMessage> Network::getReceivedMessages(Milliseconds currentTime) {
  checkStatus(currentTime);
  return getReceivedMessagesImpl(currentTime);
}

std::list<NetworkMessage> UdpNetwork::getReceivedMessagesImpl(Milliseconds currentTime) {
  std::list<NetworkMessage> receivedMessages;
  if (!maybeHandleNotConnected(currentTime)) {
    return receivedMessages;
  }
  while (true) {
    Message packet;

    // Now let's see if we received anything
    int n = recv(&packet, sizeof(packet));
    if (n <= 0) {
      break;
    }
    if (n < static_cast<int>(sizeof(packet))) {
      error("%u Received packet too short, received %d bytes, expected at least %d bytes",
            currentTime, n, sizeof(packet));
      continue;
    }
    int32_t msgcode = ntohl(packet.msgcode);
    if (msgcode == msgcodeEffectFrame) {
      if (packet.data.frame.reserved[6] != 0x33 ||
          packet.data.frame.reserved[7] != 0x77) {
        error("%u Ignoring message with bad reserved", currentTime);
        continue;;
      }
      effectLastRxTime_ = currentTime;

      if (effectLastRxTime_ - effectLastTxTime_ < 50) {
        debug("%u This must be my own packet, ignoring", currentTime);
        continue;
      }

      if (isEffectMaster_) {
        info("%u No longer effect master because of received packet", currentTime);
      }
      isEffectMaster_ = false;
      NetworkMessage receivedMessage;
      // TODO: receivedMessage.originator;
      receivedMessage.currentPattern = packet.data.frame.pattern;
      receivedMessage.nextPattern = 0; // TODO;
      receivedMessage.elapsedTime = static_cast<Milliseconds>(ntohl(packet.data.frame.time));;
      receivedMessage.precedence = 0; // TODO

      info("%u Received pattern %s, time %d ms",
          currentTime, displayBitsAsBinary(receivedMessage.currentPattern).c_str(),
          receivedMessage.elapsedTime);
      receivedMessages.push_back(receivedMessage);
      continue;
    } else {
      error("%u Received unknown message, code: %d", currentTime, msgcode);
      continue;
    }
  }
  if (!receivedMessages.empty()) {
    if (effectLastRxTime_ < 1) {
      effectLastRxTime_ = currentTime;
    }

    if (!isEffectMaster_
        && currentTime - effectLastRxTime_ > maxMissingEffectMasterPeriod) {
      info("%u Haven't heard from effect master for a while, taking over",
          currentTime);
      isEffectMaster_ = true;
    }
  }
  return receivedMessages;
}

void Network::checkStatus(Milliseconds currentTime) {
  if (status_ == CONNECTION_FAILED) {
    backoffTimeout_ = min(maxBackoffTimeout_, backoffTimeout_ * 2);
    if (currentTime - lastConnectionAttempt_ > backoffTimeout_) {
      reconnect(currentTime);
    }
  } else {
    const NetworkStatus previousStatus = status_;
    status_ = update(status_);
    if (status_ != previousStatus) {
      info("Network updated status from %s to %s",
            NetworkStatusToString(previousStatus).c_str(),
            NetworkStatusToString(status_).c_str());
    }
  }
  if (status_ == CONNECTED) {
    backoffTimeout_ = minBackoffTimeout_;
  }
}


void Network::runLoop(Milliseconds currentTime) {
  checkStatus(currentTime);
  runLoopImpl(currentTime);
}

bool UdpNetwork::maybeHandleNotConnected(Milliseconds currentTime) {
  if (status() == CONNECTED) {
    return true;
  }
  if (isEffectMaster_) {
    info("%u No longer effect master because of network disconnection",
          currentTime);
    isEffectMaster_ = false;
  }
  effectLastRxTime_ = 0;
  return false;
}

void UdpNetwork::runLoopImpl(Milliseconds currentTime) {
  if (!maybeHandleNotConnected(currentTime)) {
    return;
  }

  // Do we need to broadcast?
  if (isEffectMaster_ && (effectLastTxTime_ < 1
                          || currentTime - effectLastTxTime_ > effectUpdatePeriod
                          || messageToSend_.currentPattern != lastSentPattern_)) {
    Message packet;
    memset(&packet, 0, sizeof(packet));
    packet.data.frame.reserved[6] = 0x33;
    packet.data.frame.reserved[7] = 0x77;
    packet.msgcode = htonl(msgcodeEffectFrame);
    packet.data.frame.pattern = messageToSend_.currentPattern;
    lastSentPattern_ = messageToSend_.currentPattern;
    packet.data.frame.time = htonl(messageToSend_.elapsedTime);
    effectLastTxTime_ = currentTime;
    send(&packet, sizeof(packet));
    info("%u Sent pattern %s, time %d ms",
         currentTime, displayBitsAsBinary(messageToSend_.currentPattern).c_str(),
         messageToSend_.elapsedTime);
  }
}

} // namespace unisparks
