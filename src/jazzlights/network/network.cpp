#include "jazzlights/network/network.h"

#include <stdint.h>
#include <string.h>

#include <atomic>

#include "jazzlights/protocol/wire_types.h"
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

void WriteUint32(uint8_t* data, uint32_t number) {
  data[0] = static_cast<uint8_t>((number & 0xFF000000) >> 24);
  data[1] = static_cast<uint8_t>((number & 0x00FF0000) >> 16);
  data[2] = static_cast<uint8_t>((number & 0x0000FF00) >> 8);
  data[3] = static_cast<uint8_t>((number & 0x000000FF));
}

void WriteUint16(uint8_t* data, uint16_t number) {
  data[0] = static_cast<uint8_t>((number & 0xFF00) >> 8);
  data[1] = static_cast<uint8_t>((number & 0x00FF));
}

uint32_t ReadUint32(const uint8_t* data) { return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]); }

uint16_t ReadUint16(const uint8_t* data) { return (data[0] << 8) | (data[1]); }

std::string NetworkStatusToString(NetworkStatus status) {
  switch (status) {
#define X(s) \
  case k##s: return #s;
    ALL_NETWORK_STATUSES
#undef X
  }
  return "UNKNOWN";
}

NetworkId Network::NextAvailableId() {
  static std::atomic<NetworkId> nextId{1};
  NetworkId res = nextId.fetch_add(1, std::memory_order_relaxed);
  if (res == std::numeric_limits<NetworkId>::max() - 1) { jll_fatal("Ran out of available NetworkIds"); }
  return res;
}

NetworkStatus Network::Status() const { return status_; }

void Network::Reconnect() {
  if (status_ != kConnected) {
    lastConnectionAttempt_ = TimeMicros();
    jll_info("%s Network Reconnecting", NetworkTypeToString(Type()));
    status_ = Update(kConnecting);
  }
}

void UdpNetwork::TriggerSendAsap() {
  effectLastTxTime_ = 0;
  RunLoop();
}

void UdpNetwork::SetMessageToSend(const ProtocolMessage& messageToSend) {
  hasDataToSend_ = true;
  messageToSend_ = messageToSend;
}

void UdpNetwork::DisableSending() { hasDataToSend_ = false; }

std::list<ProtocolMessage> Network::GetReceivedMessages() {
  CheckStatus();
  std::list<ProtocolMessage> receivedMessages = GetReceivedMessagesImpl();
  for (ProtocolMessage& message : receivedMessages) {
    message.receiptNetworkId = id();
    message.receiptNetworkType = Type();
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
                              ProtocolMessage* outMessage) {
  Microseconds currentTime = TimeMicros();
  if (udpPayloadLength < kPayloadLength) {
    jll_debug("%s Received packet too short, received %zd bytes, expected at least %zu bytes",
              NetworkTypeToString(Type()), udpPayloadLength, kPayloadLength);
    return false;
  }
  if ((udpPayload[kVersionOffset] & 0xF0) != kVersion) {
    jll_debug("%s Received packet with unexpected prefix %02x", NetworkTypeToString(Type()),
              udpPayload[kVersionOffset]);
    return false;
  }
  ProtocolMessage receivedMessage;
  receivedMessage.originator = NetworkDeviceId(&udpPayload[kOriginatorOffset]);
  receivedMessage.sender = NetworkDeviceId(&udpPayload[kSenderOffset]);
  receivedMessage.precedence = ReadUint16(&udpPayload[kPrecedenceOffset]);
  receivedMessage.numHops = udpPayload[kNumHopsOffset];
  Microseconds originationTimeDelta = MillisecondsToMicroseconds(ReadUint16(&udpPayload[kOriginationTimeOffset]));
  receivedMessage.currentPattern = ReadUint32(&udpPayload[kCurrentPatternOffset]);
  receivedMessage.nextPattern = ReadUint32(&udpPayload[kNextPatternOffset]);
  Microseconds patternTimeDelta = MillisecondsToMicroseconds(ReadUint16(&udpPayload[kPatternTimeOffset]));
  receivedMessage.receiptDetails = receiptDetails;

  // TODO measure transmission offset over various underlying UDP networks like Wi-Fi and Ethernet.
  static constexpr Microseconds kTransmissionOffset = 5 * kMicrosecondsPerMillisecond;  // 5ms.
  Microseconds receiptTime;
  if (currentTime > kTransmissionOffset) {
    receiptTime = currentTime - kTransmissionOffset;
  } else {
    receiptTime = 0;
  }
  if (receiptTime > patternTimeDelta) {
    receivedMessage.currentPatternStartTime = receiptTime - patternTimeDelta;
  } else {
    receivedMessage.currentPatternStartTime = 0;
  }
  if (receiptTime > originationTimeDelta) {
    receivedMessage.lastOriginationTime = receiptTime - originationTimeDelta;
  } else {
    receivedMessage.lastOriginationTime = 0;
  }

  jll_debug("%s received %s", NetworkTypeToString(Type()), NetworkMessageToString(receivedMessage).c_str());

  *outMessage = receivedMessage;
  return true;
}

std::list<ProtocolMessage> UdpNetwork::GetReceivedMessagesImpl() {
  std::list<ProtocolMessage> receivedMessages;
  if (Status() != kConnected) { return receivedMessages; }
  Microseconds currentTime = TimeMicros();
  while (true) {
    uint8_t udpPayload[2000] = {};
    std::string receiptDetails;
    ssize_t n = Recv(&udpPayload[0], sizeof(udpPayload), &receiptDetails);
    if (n <= 0) { break; }
    ProtocolMessage receivedMessage;
    if (!ParseUdpPayload(udpPayload, n, receiptDetails, &receivedMessage)) { continue; }
    receivedMessages.push_back(receivedMessage);
    lastReceiveTime_ = currentTime;
  }
  return receivedMessages;
}

void Network::CheckStatus() {
  Microseconds currentTime = TimeMicros();
  if (status_ == kConnectionFailed) {
    backoffTimeout_ = std::min(MaxBackoffTimeout(), backoffTimeout_ * 2);
    if (currentTime - lastConnectionAttempt_ > backoffTimeout_) { Reconnect(); }
  } else {
    const NetworkStatus previousStatus = status_;
    status_ = Update(status_);
    if (status_ != previousStatus) {
      jll_info("%s updated status from %s to %s", NetworkTypeToString(Type()),
               NetworkStatusToString(previousStatus).c_str(), NetworkStatusToString(status_).c_str());
    }
  }
  if (status_ == kConnected) { backoffTimeout_ = MinBackoffTimeout(); }
}

void Network::RunLoop() {
  CheckStatus();
  RunLoopImpl();
}

bool Network::WriteUdpPayload(const ProtocolMessage& messageToSend, uint8_t* udpPayload, size_t udpPayloadLength) {
  if (udpPayloadLength < kPayloadLength) {
    jll_error("%s cannot send message due to payload too short %zu < %zu", NetworkTypeToString(Type()),
              udpPayloadLength, kPayloadLength);
    return false;
  }

  Microseconds currentTime = TimeMicros();
  uint16_t originationTimeDeltaMs16;
  if (messageToSend.lastOriginationTime >= currentTime) {
    originationTimeDeltaMs16 = 0;
  } else if (currentTime - messageToSend.lastOriginationTime < kMicrosecondsPerMillisecond * 0xFFFF) {
    originationTimeDeltaMs16 = (currentTime - messageToSend.lastOriginationTime) / kMicrosecondsPerMillisecond;
  } else {
    originationTimeDeltaMs16 = 0xFFFF;
  }
  uint16_t patternTimeDeltaMs16;
  if (messageToSend.currentPatternStartTime >= currentTime) {
    patternTimeDeltaMs16 = 0;
  } else if (currentTime - messageToSend.currentPatternStartTime < kMicrosecondsPerMillisecond * 0xFFFF) {
    patternTimeDeltaMs16 = (currentTime - messageToSend.currentPatternStartTime) / kMicrosecondsPerMillisecond;
  } else {
    patternTimeDeltaMs16 = 0xFFFF;
  }
  jll_debug("%s sending %s", NetworkTypeToString(Type()), NetworkMessageToString(messageToSend).c_str());

  udpPayload[kVersionOffset] = kVersion;
  messageToSend.originator.WriteTo(&udpPayload[kOriginatorOffset]);
  messageToSend.sender.WriteTo(&udpPayload[kSenderOffset]);
  WriteUint16(&udpPayload[kPrecedenceOffset], messageToSend.precedence);
  udpPayload[kNumHopsOffset] = messageToSend.numHops;
  WriteUint16(&udpPayload[kOriginationTimeOffset], originationTimeDeltaMs16);
  WriteUint32(&udpPayload[kCurrentPatternOffset], messageToSend.currentPattern);
  WriteUint32(&udpPayload[kNextPatternOffset], messageToSend.nextPattern);
  WriteUint16(&udpPayload[kPatternTimeOffset], patternTimeDeltaMs16);
  return true;
}

void UdpNetwork::RunLoopImpl() {
  if (Status() != kConnected) { return; }

  // Do we need to send?
  Microseconds currentTime = TimeMicros();
  static constexpr Microseconds kMinTimeBetweenUdpSends = 100 * kMicrosecondsPerMillisecond;
  if (hasDataToSend_ && (effectLastTxTime_ < 1 || currentTime - effectLastTxTime_ > kMinTimeBetweenUdpSends ||
                         messageToSend_.currentPattern != lastSentPattern_)) {
    effectLastTxTime_ = currentTime;
    lastSentPattern_ = messageToSend_.currentPattern;

    uint8_t udpPayload[kPayloadLength] = {};
    if (!WriteUdpPayload(messageToSend_, udpPayload, sizeof(udpPayload))) {
      jll_fatal("%s unexpected payload length issue", NetworkTypeToString(Type()));
    }
    Send(&udpPayload[0], sizeof(udpPayload));
  }
}

}  // namespace jazzlights
