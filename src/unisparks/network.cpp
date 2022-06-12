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

void writeUint32(uint8_t* data, uint32_t number) {
  data[0] = static_cast<uint8_t>((number & 0xFF000000) >> 24);
  data[1] = static_cast<uint8_t>((number & 0x00FF0000) >> 16);
  data[2] = static_cast<uint8_t>((number & 0x0000FF00) >> 8);
  data[3] = static_cast<uint8_t>((number & 0x000000FF));
}

void writeUint16(uint8_t* data, uint16_t number) {
  data[0] = static_cast<uint8_t>((number & 0xFF00) >> 8);
  data[1] = static_cast<uint8_t>((number & 0x00FF));
}

uint32_t readUint32(const uint8_t* data) {
  return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);
}

uint16_t readUint16(const uint8_t* data) {
  return (data[0] << 8) | (data[1]);
}

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

std::string networkMessageToString(const NetworkMessage& message, Milliseconds currentTime) {
  char str[sizeof(", t=4294967296, p=65536, nh=255, ot=4294967296}")] = {};
  snprintf(str, sizeof(str), ", t=%u, p=%u, nh=%u, ot=%u}",
           currentTime - message.currentPatternStartTime,
           message.precedence, message.numHops,
           currentTime - message.lastOriginationTime);
  std::string rv = "{o=" + message.originator.toString() +
                   ", s=" + message.sender.toString() +
                   ", c=" + displayBitsAsBinary(message.currentPattern) +
                   ", n=" + displayBitsAsBinary(message.nextPattern);
  if (message.receiptNetwork != nullptr) {
    rv += ", ";
    rv += message.receiptNetwork->name();
  }
  rv += str;
  return rv;
}

NetworkStatus Network::status() const {
  return status_;
}

void Network::reconnect(Milliseconds currentTime) {
  if (status_ != CONNECTED) {
    lastConnectionAttempt_ = currentTime;
    info("%u %s Network Reconnecting", currentTime, name());
    status_ = update(CONNECTING, currentTime);
  }
}

void UdpNetwork::triggerSendAsap(Milliseconds currentTime) {
  effectLastTxTime_ = 0;
  runLoop(currentTime);
}

void UdpNetwork::setMessageToSend(const NetworkMessage& messageToSend,
                                  Milliseconds /*currentTime*/) {
  hasDataToSend_ = true;
  messageToSend_ = messageToSend;
}

void UdpNetwork::disableSending(Milliseconds /*currentTime*/) {
  hasDataToSend_ = false;
}

std::list<NetworkMessage> Network::getReceivedMessages(Milliseconds currentTime) {
  checkStatus(currentTime);
  std::list<NetworkMessage> receivedMessages = getReceivedMessagesImpl(currentTime);
  for (NetworkMessage& message : receivedMessages) {
    message.receiptNetwork = this;
  }
  return receivedMessages;
}

constexpr uint8_t kVersion = 0x10;
constexpr uint8_t kVersionOffset = 0;
constexpr uint8_t kOriginatorOffset = kVersionOffset + 1;
constexpr uint8_t kSenderOffset = kOriginatorOffset + 6;
constexpr uint8_t kPrecedenceOffset = kSenderOffset + 6;
constexpr uint8_t kNumHopsOffset = kPrecedenceOffset + 2;
constexpr uint8_t kOriginationTimeOffset = kNumHopsOffset + 1;
constexpr uint8_t kCurrentPatternOffset = kOriginationTimeOffset + 2;
constexpr uint8_t kNextPatternOffset = kCurrentPatternOffset + 4;
constexpr uint8_t kPatternTimeOffset = kNextPatternOffset + 4;
constexpr uint8_t kPayloadLength = kPatternTimeOffset + 2;

std::list<NetworkMessage> UdpNetwork::getReceivedMessagesImpl(Milliseconds currentTime) {
  std::list<NetworkMessage> receivedMessages;
  if (!maybeHandleNotConnected(currentTime)) {
    return receivedMessages;
  }
  while (true) {
    uint8_t udpPayload[2000] = {};
    ssize_t n = recv(&udpPayload[0], sizeof(udpPayload));
    if (n <= 0) {
      break;
    }
    if (n < kPayloadLength) {
      info("%u %s Received packet too short, received %d bytes, expected at least %d bytes",
           currentTime, name(), n, kPayloadLength);
      continue;
    }
    if ((udpPayload[kVersionOffset] & 0xF0) != kVersion) {
      info("%u %s Received packet with unexpected prefix %02x",
           currentTime, name(), udpPayload[kVersionOffset]);
      continue;
    }
    NetworkMessage receivedMessage;
    receivedMessage.originator = NetworkDeviceId(&udpPayload[kOriginatorOffset]);
    receivedMessage.sender = NetworkDeviceId(&udpPayload[kSenderOffset]);
    receivedMessage.precedence = readUint16(&udpPayload[kPrecedenceOffset]);
    receivedMessage.numHops = udpPayload[kNumHopsOffset];
    Milliseconds originationTimeDelta = readUint16(&udpPayload[kOriginationTimeOffset]);
    receivedMessage.currentPattern = readUint32(&udpPayload[kCurrentPatternOffset]);
    receivedMessage.nextPattern = readUint32(&udpPayload[kNextPatternOffset]);
    Milliseconds patternTimeDelta = readUint16(&udpPayload[kPatternTimeOffset]);

    // TODO measure transmission offset over Wi-Fi.
    constexpr Milliseconds kTransmissionOffset = 5;
    Milliseconds receiptTime;
    if (currentTime > kTransmissionOffset) {
      receiptTime = currentTime - kTransmissionOffset;
    } else {
      receiptTime = 0;
    }
    if (receiptTime >= patternTimeDelta) {
      receivedMessage.currentPatternStartTime = receiptTime - patternTimeDelta;
    } else {
      receivedMessage.currentPatternStartTime = 0;
    }
    if (receiptTime >= originationTimeDelta) {
      receivedMessage.lastOriginationTime = receiptTime - originationTimeDelta;
    } else {
      receivedMessage.lastOriginationTime = 0;
    }

    debug("%u %s received %s",
          currentTime, name(),
          networkMessageToString(receivedMessage, currentTime).c_str());
    receivedMessages.push_back(receivedMessage);
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
    status_ = update(status_, currentTime);
    if (status_ != previousStatus) {
      info("%s updated status from %s to %s",
           name(),
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

bool UdpNetwork::maybeHandleNotConnected(Milliseconds /*currentTime*/) {
  return status() == CONNECTED;
}

void UdpNetwork::runLoopImpl(Milliseconds currentTime) {
  if (!maybeHandleNotConnected(currentTime)) {
    return;
  }

  // Do we need to send?
  static constexpr Milliseconds kMinTimeBetweenUdpSends = 100;
  if (hasDataToSend_ &&
      (effectLastTxTime_ < 1 ||
         currentTime - effectLastTxTime_ > kMinTimeBetweenUdpSends ||
         messageToSend_.currentPattern != lastSentPattern_)) {
    effectLastTxTime_ = currentTime;
    lastSentPattern_ = messageToSend_.currentPattern;
    Milliseconds originationTimeDelta;
    if (messageToSend_.lastOriginationTime <= currentTime &&
        currentTime - messageToSend_.lastOriginationTime <= 0xFFFF) {
      originationTimeDelta = currentTime - messageToSend_.lastOriginationTime;
    } else {
      originationTimeDelta = 0xFFFF;
    }
    Milliseconds patternTime;
    if (messageToSend_.currentPatternStartTime <= currentTime &&
        currentTime - messageToSend_.currentPatternStartTime <= 0xFFFF) {
      patternTime = currentTime - messageToSend_.currentPatternStartTime;
    } else {
      patternTime = 0xFFFF;
    }
    debug("%u %s sending %s",
          currentTime, name(),
          networkMessageToString(messageToSend_, currentTime).c_str());

    uint8_t udpPayload[kPayloadLength] = {};
    udpPayload[kVersionOffset] = kVersion;
    messageToSend_.originator.writeTo(&udpPayload[kOriginatorOffset]);
    messageToSend_.sender.writeTo(&udpPayload[kSenderOffset]);
    writeUint16(&udpPayload[kPrecedenceOffset], messageToSend_.precedence);
    udpPayload[kNumHopsOffset] = messageToSend_.numHops;
    writeUint16(&udpPayload[kOriginationTimeOffset], originationTimeDelta);
    writeUint32(&udpPayload[kCurrentPatternOffset], messageToSend_.currentPattern);
    writeUint32(&udpPayload[kNextPatternOffset], messageToSend_.nextPattern);
    writeUint16(&udpPayload[kPatternTimeOffset], patternTime);
    send(&udpPayload[0], sizeof(udpPayload));
  }
}

} // namespace unisparks
