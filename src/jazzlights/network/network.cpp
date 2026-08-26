#include "jazzlights/network/network.h"

#include <stdint.h>
#include <string.h>

#include <atomic>

#include "jazzlights/protocol/wire.h"
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

bool Network::ParseUdpPayload(uint8_t* udpPayload, size_t udpPayloadLength, const std::string& receiptDetails,
                              ProtocolMessage* outMessage) {
  // TODO measure transmission offset over various underlying UDP networks like Wi-Fi and Ethernet.
  static constexpr Microseconds kTransmissionOffset = 5 * kMicrosecondsPerMillisecond;  // 5ms.
  const Microseconds receiptTime = TimeMicros() - kTransmissionOffset;
  std::optional<ProtocolMessage> parsedMessage =
      ParseProtocolMessage(udpPayload, udpPayloadLength, /*isBle=*/false, receiptTime);
  if (!parsedMessage) { return false; }
  parsedMessage->receiptDetails = receiptDetails;

  jll_debug("%s received %s", NetworkTypeToString(Type()), NetworkMessageToString(*parsedMessage).c_str());

  *outMessage = *parsedMessage;
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

size_t Network::WriteUdpPayload(const ProtocolMessage& messageToSend, uint8_t* udpPayload, size_t udpPayloadLength) {
  if (udpPayloadLength < kUdpProtocolPayloadLength) {
    jll_error("%s cannot send message due to payload too short %zu < %zu", NetworkTypeToString(Type()),
              udpPayloadLength, kUdpProtocolPayloadLength);
    return 0;
  }
  jll_debug("%s sending %s", NetworkTypeToString(Type()), NetworkMessageToString(messageToSend).c_str());
  return WriteProtocolMessage(messageToSend, /*isBle=*/false, udpPayload, udpPayloadLength) != 0;
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

    uint8_t udpPayload[kUdpProtocolPayloadLength] = {};
    size_t payloadLength = WriteUdpPayload(messageToSend_, udpPayload, sizeof(udpPayload));
    if (payloadLength == 0) {
      jll_error("%s failed to write", NetworkTypeToString(Type()));
      return;
    }
    Send(&udpPayload[0], payloadLength);
  }
}

}  // namespace jazzlights
