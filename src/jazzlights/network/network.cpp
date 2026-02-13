#include "jazzlights/network/network.h"

#include <stdint.h>
#include <string.h>

#include <atomic>

#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"
#ifndef ESP32
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
#else  // ESP32
#ifndef ntohl
constexpr uint32_t ntohl(uint32_t n) {
  return ((n & 0xFF) << 24) | ((n & 0xFF00) << 8) | ((n & 0xFF0000) >> 8) | ((n & 0xFF000000) >> 24);
}
#endif  // ntohl
#ifndef htonl
constexpr uint32_t htonl(uint32_t n) {
  return ((n & 0xFF) << 24) | ((n & 0xFF00) << 8) | ((n & 0xFF0000) >> 8) | ((n & 0xFF000000) >> 24);
}
#endif  // htonl
#endif  // ESP32

namespace jazzlights {

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

uint32_t readUint32(const uint8_t* data) { return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]); }

uint16_t readUint16(const uint8_t* data) { return (data[0] << 8) | (data[1]); }

const char* NetworkTypeToString(NetworkType type) {
  switch (type) {
    case NetworkType::kLeading: return "Lead";
    case NetworkType::kBLE: return "BLE";
    case NetworkType::kWiFi: return "WiFi";
    case NetworkType::kEthernet: return "Eth";
    case NetworkType::kOther: return "Other";
  }
  return "???";
}

std::string NetworkStatusToString(NetworkStatus status) {
  switch (status) {
#define X(s) \
  case s: return #s;
    ALL_NETWORK_STATUSES
#undef X
  }
  return "UNKNOWN";
}

std::string displayBitsAsBinary(PatternBits p) {
  static_assert(sizeof(p) == 4, "32bits");
  char bits[33] = {};
  for (uint8_t b = 0; b < 32; b++) {
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
  snprintf(str, sizeof(str), ", t=%u, p=%u, nh=%u, ot=%u}", currentTime - message.currentPatternStartTime,
           message.precedence, message.numHops, currentTime - message.lastOriginationTime);
  std::string rv = "{o=" + message.originator.toString() + ", s=" + message.sender.toString();
#if !JL_IS_CONFIG(CREATURE)
  rv += ", c=" + displayBitsAsBinary(message.currentPattern);
  rv += ", n=" + displayBitsAsBinary(message.nextPattern);
#endif  // !CREATURE
  if (message.receiptNetworkType != NetworkType::kLeading) {
    rv += ", ";
    rv += NetworkTypeToString(message.receiptNetworkType);
  }
#if JL_IS_CONFIG(CREATURE)
  char str2[sizeof(", rssi=-2147483648, rgb=010203")] = {};
  snprintf(str2, sizeof(str2), ", rssi=%d, rgb=%06x", message.receiptRssi, static_cast<int>(message.creatureColor));
  rv += str2;
#endif  // CREATURE
  rv += str;
  return rv;
}

NetworkDeviceId NetworkDeviceId::PlusOne() const {
  NetworkDeviceId deviceId = *this;
  deviceId.data()[5]++;
  if (deviceId.data()[5] == 0) {
    deviceId.data()[4]++;
    if (deviceId.data()[4] == 0) {
      deviceId.data()[3]++;
      if (deviceId.data()[3] == 0) {
        deviceId.data()[2]++;
        if (deviceId.data()[2] == 0) {
          deviceId.data()[1]++;
          if (deviceId.data()[1] == 0) { deviceId.data()[0]++; }
        }
      }
    }
  }
  return deviceId;
}

NetworkId Network::NextAvailableId() {
  static std::atomic<NetworkId> nextId{1};
  NetworkId res = nextId.fetch_add(1, std::memory_order_relaxed);
  if (res == std::numeric_limits<NetworkId>::max() - 1) { jll_fatal("Ran out of available NetworkIds"); }
  return res;
}

NetworkStatus Network::status() const { return status_; }

void Network::reconnect(Milliseconds currentTime) {
  if (status_ != CONNECTED) {
    lastConnectionAttempt_ = currentTime;
    jll_info("%u %s Network Reconnecting", currentTime, NetworkTypeToString(type()));
    status_ = update(CONNECTING, currentTime);
  }
}

void UdpNetwork::triggerSendAsap(Milliseconds currentTime) {
  effectLastTxTime_ = 0;
  runLoop(currentTime);
}

void UdpNetwork::setMessageToSend(const NetworkMessage& messageToSend, Milliseconds /*currentTime*/) {
  hasDataToSend_ = true;
  messageToSend_ = messageToSend;
}

void UdpNetwork::disableSending(Milliseconds /*currentTime*/) { hasDataToSend_ = false; }

std::list<NetworkMessage> Network::getReceivedMessages(Milliseconds currentTime) {
  checkStatus(currentTime);
  std::list<NetworkMessage> receivedMessages = getReceivedMessagesImpl(currentTime);
  for (NetworkMessage& message : receivedMessages) {
    message.receiptNetworkId = id();
    message.receiptNetworkType = type();
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
constexpr size_t kPayloadLength = kPatternTimeOffset + 2;

bool Network::ParseUdpPayload(uint8_t* udpPayload, size_t udpPayloadLength, const std::string& receiptDetails,
                              Milliseconds currentTime, NetworkMessage* outMessage) {
  if (udpPayloadLength < kPayloadLength) {
    jll_debug("%u %s Received packet too short, received %zd bytes, expected at least %zu bytes", currentTime,
              NetworkTypeToString(type()), udpPayloadLength, kPayloadLength);
    return false;
  }
  if ((udpPayload[kVersionOffset] & 0xF0) != kVersion) {
    jll_debug("%u %s Received packet with unexpected prefix %02x", currentTime, NetworkTypeToString(type()),
              udpPayload[kVersionOffset]);
    return false;
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
  receivedMessage.receiptDetails = receiptDetails;

  // TODO measure transmission offset over various underlying UDP networks like Wi-Fi and Ethernet.
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

  jll_debug("%u %s received %s", currentTime, NetworkTypeToString(type()),
            networkMessageToString(receivedMessage, currentTime).c_str());

  *outMessage = receivedMessage;
  return true;
}

std::list<NetworkMessage> UdpNetwork::getReceivedMessagesImpl(Milliseconds currentTime) {
  std::list<NetworkMessage> receivedMessages;
  if (status() != CONNECTED) { return receivedMessages; }
  while (true) {
    uint8_t udpPayload[2000] = {};
    std::string receiptDetails;
    ssize_t n = recv(&udpPayload[0], sizeof(udpPayload), &receiptDetails);
    if (n <= 0) { break; }
    NetworkMessage receivedMessage;
    if (!ParseUdpPayload(udpPayload, n, receiptDetails, currentTime, &receivedMessage)) { continue; }
    receivedMessages.push_back(receivedMessage);
    lastReceiveTime_ = currentTime;
  }
  return receivedMessages;
}

void Network::checkStatus(Milliseconds currentTime) {
  if (status_ == CONNECTION_FAILED) {
    backoffTimeout_ = std::min(MaxBackoffTimeout(), backoffTimeout_ * 2);
    if (currentTime - lastConnectionAttempt_ > backoffTimeout_) { reconnect(currentTime); }
  } else {
    const NetworkStatus previousStatus = status_;
    status_ = update(status_, currentTime);
    if (status_ != previousStatus) {
      jll_info("%u %s updated status from %s to %s", currentTime, NetworkTypeToString(type()),
               NetworkStatusToString(previousStatus).c_str(), NetworkStatusToString(status_).c_str());
    }
  }
  if (status_ == CONNECTED) { backoffTimeout_ = MinBackoffTimeout(); }
}

void Network::runLoop(Milliseconds currentTime) {
  checkStatus(currentTime);
  runLoopImpl(currentTime);
}

bool Network::WriteUdpPayload(const NetworkMessage& messageToSend, uint8_t* udpPayload, size_t udpPayloadLength,
                              Milliseconds currentTime) {
  if (udpPayloadLength < kPayloadLength) {
    jll_error("%u %s cannot send message due to payload too short %zu < %zu", currentTime, NetworkTypeToString(type()),
              udpPayloadLength, kPayloadLength);
    return false;
  }

  Milliseconds originationTimeDelta;
  if (messageToSend.lastOriginationTime <= currentTime && currentTime - messageToSend.lastOriginationTime <= 0xFFFF) {
    originationTimeDelta = currentTime - messageToSend.lastOriginationTime;
  } else {
    originationTimeDelta = 0xFFFF;
  }
  Milliseconds patternTime;
  if (messageToSend.currentPatternStartTime <= currentTime &&
      currentTime - messageToSend.currentPatternStartTime <= 0xFFFF) {
    patternTime = currentTime - messageToSend.currentPatternStartTime;
  } else {
    patternTime = 0xFFFF;
  }
  jll_debug("%u %s sending %s", currentTime, NetworkTypeToString(type()),
            networkMessageToString(messageToSend, currentTime).c_str());

  udpPayload[kVersionOffset] = kVersion;
  messageToSend.originator.writeTo(&udpPayload[kOriginatorOffset]);
  messageToSend.sender.writeTo(&udpPayload[kSenderOffset]);
  writeUint16(&udpPayload[kPrecedenceOffset], messageToSend.precedence);
  udpPayload[kNumHopsOffset] = messageToSend.numHops;
  writeUint16(&udpPayload[kOriginationTimeOffset], originationTimeDelta);
  writeUint32(&udpPayload[kCurrentPatternOffset], messageToSend.currentPattern);
  writeUint32(&udpPayload[kNextPatternOffset], messageToSend.nextPattern);
  writeUint16(&udpPayload[kPatternTimeOffset], patternTime);
  return true;
}

void UdpNetwork::runLoopImpl(Milliseconds currentTime) {
  if (status() != CONNECTED) { return; }

  // Do we need to send?
  static constexpr Milliseconds kMinTimeBetweenUdpSends = 100;
  if (hasDataToSend_ && (effectLastTxTime_ < 1 || currentTime - effectLastTxTime_ > kMinTimeBetweenUdpSends ||
                         messageToSend_.currentPattern != lastSentPattern_)) {
    effectLastTxTime_ = currentTime;
    lastSentPattern_ = messageToSend_.currentPattern;

    uint8_t udpPayload[kPayloadLength] = {};
    if (!WriteUdpPayload(messageToSend_, udpPayload, sizeof(udpPayload), currentTime)) {
      jll_fatal("%s unexpected payload length issue", NetworkTypeToString(type()));
    }
    send(&udpPayload[0], sizeof(udpPayload));
  }
}

}  // namespace jazzlights
