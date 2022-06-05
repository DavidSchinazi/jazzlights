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
  char str[sizeof(", t=4294967296, p=65536}")] = {};
  snprintf(str, sizeof(str), ", t=%u, p=%u}",
           currentTime - message.currentPatternStartTime,
           message.precedence);
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

void UdpNetwork::triggerSendAsap(Milliseconds /*currentTime*/) {
  effectLastTxTime_ = 0;
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
    if (n < 1 + 6 + 6 + 2 + 4 + 4 + 2) {
      info("%u %s Received packet too short, received %d bytes, expected at least %d bytes",
           currentTime, name(), n, 1 + 6 + 6 + 2 + 4 + 4 + 2);
      continue;
    }
    if ((udpPayload[0] & 0xF0) != 0) {
      info("%u %s Received packet with unexpected prefix %02x",
           currentTime, name(), udpPayload[0]);
      continue;
    }
    NetworkMessage receivedMessage;
    receivedMessage.originator = NetworkDeviceId(&udpPayload[1]);
    receivedMessage.sender = NetworkDeviceId(&udpPayload[1 + 6]);
    receivedMessage.precedence = readUint16(&udpPayload[1 + 6 + 6]);
    receivedMessage.currentPattern = readUint32(&udpPayload[1 + 6 + 6 + 2]);
    receivedMessage.nextPattern = readUint32(&udpPayload[1 + 6 + 6 + 2 + 4]);
    Milliseconds elapsedTime = readUint16(&udpPayload[1 + 6 + 6 + 2 + 4 + 4]);
    receivedMessage.currentPatternStartTime = currentTime - elapsedTime;
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
    Milliseconds elapsedTime;
    if (messageToSend_.currentPatternStartTime <= currentTime &&
        currentTime - messageToSend_.currentPatternStartTime <= 0xFFFF) {
      elapsedTime = currentTime - messageToSend_.currentPatternStartTime;
    } else {
      elapsedTime = 0xFFFF;
    }
    debug("%u %s sending %s",
          currentTime, name(),
          networkMessageToString(messageToSend_, currentTime).c_str());

    uint8_t udpPayload[1 + 6 + 6 + 2 + 4 + 4 + 2] = {};
    messageToSend_.originator.writeTo(&udpPayload[1]);
    messageToSend_.sender.writeTo(&udpPayload[1 + 6]);
    writeUint16(&udpPayload[1 + 6 + 6], messageToSend_.precedence);
    writeUint32(&udpPayload[1 + 6 + 6 + 2], messageToSend_.currentPattern);
    writeUint32(&udpPayload[1 + 6 + 6 + 2 + 4], messageToSend_.nextPattern);
    writeUint16(&udpPayload[1 + 6 + 6 + 2 + 4 + 4], elapsedTime);
    send(&udpPayload[0], sizeof(udpPayload));
  }
}

} // namespace unisparks
