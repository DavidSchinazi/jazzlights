#ifndef JL_NETWORK_NETWORK_H
#define JL_NETWORK_NETWORK_H

#include <limits>
#include <list>
#include <string>

#include "jazzlights/protocol/wire_types.h"
#include "jazzlights/util/config.h"
#include "jazzlights/util/time.h"
#include "jazzlights/util/types.h"

namespace jazzlights {

#define ALL_NETWORK_STATUSES \
  X(Initializing)            \
  X(Connecting)              \
  X(Connected)               \
  X(ConnectionFailed)

enum NetworkStatus {
#define X(s) k##s,
  ALL_NETWORK_STATUSES
#undef X
};

std::string NetworkStatusToString(NetworkStatus status);

class Network {
 public:
  // Disallow copy and move.
  Network(const Network&) = delete;
  Network(Network&&) = delete;
  Network& operator=(const Network&) = delete;
  Network& operator=(Network&&) = delete;

  virtual ~Network() = default;

  // Set message to send during next send opportunity.
  virtual void SetMessageToSend(const ProtocolMessage& messageToSend) = 0;

  // Disables sending until the next call to SetMessageToSend.
  virtual void DisableSending() = 0;

  // Gets list of received messages since last call.
  std::list<ProtocolMessage> GetReceivedMessages();

  // Called once per primary runloop.
  void RunLoop();

  // Get current network status.
  NetworkStatus Status() const;

  // Get unique identifier for this network.
  NetworkId id() const { return id_; }

  // Request an immediate send.
  virtual void TriggerSendAsap() = 0;

  // Returns this device's unique ID, often using its MAC address.
  virtual NetworkDeviceId GetLocalDeviceId() const = 0;

  // The type of this network.
  virtual NetworkType Type() const = 0;

  // Whether we should advertise patterns on this network if that's where we received them.
  virtual bool ShouldEcho() const = 0;

  // Last time we received a message, or nullopt to indicate never.
  virtual OptionalMicroseconds GetLastReceiveTime() const = 0;

  // Get a human-readable status string that can be displayed to the user. Not const to allow taking locks.
  virtual std::string GetStatusStr() = 0;

 protected:
  Network() = default;
  // Perform any work necessary to switch to requested state.
  virtual NetworkStatus Update(NetworkStatus status) = 0;
  // Gets list of received messages since last call.
  virtual std::list<ProtocolMessage> GetReceivedMessagesImpl() = 0;
  // Called once per primary runloop.
  virtual void RunLoopImpl() = 0;
  NetworkStatus GetStatus() const { return status_; }
  // Default address and port for sync packets over IP.
  static constexpr uint16_t DefaultUdpPort() {
    // We intentionally squat on the babel-dtls port. Hopefully Juliusz won't mind.
    return 6699;
  }
  static constexpr const char* DefaultMulticastAddress() {
    // We intentionally squat on an unused address.
    // https://www.iana.org/assignments/multicast-addresses/multicast-addresses.xhtml#multicast-addresses-1
    return "224.0.0.169";
  }
  static constexpr const char* WiFiSsid() { return "JazzLights"; }
  static constexpr const char* WiFiPassword() { return "burningblink"; }

  // Parse the UDP payload we use over IP networks into a ProtocolMessage.
  bool ParseUdpPayload(uint8_t* udpPayload, size_t udpPayloadLength, const std::string& receiptDetails,
                       ProtocolMessage* outMessage);

  // Write a ProtocolMessage into a buffer that can be sent over UDP/IP.
  bool WriteUdpPayload(const ProtocolMessage& messageToSend, uint8_t* udpPayload, size_t udpPayloadLength);

 private:
  void CheckStatus();
  void Reconnect();

  static NetworkId NextAvailableId();

  const NetworkId id_ = NextAvailableId();

  NetworkStatus status_ = kInitializing;

  Microseconds lastConnectionAttempt_ = 0;
  static constexpr Microseconds MinBackoffTimeout() { return 1000 * kMicrosecondsPerMillisecond; }
  static constexpr Microseconds MaxBackoffTimeout() { return 16000 * kMicrosecondsPerMillisecond; }
  Microseconds backoffTimeout_ = MinBackoffTimeout();
};

class UdpNetwork : public Network {
 public:
  void SetMessageToSend(const ProtocolMessage& messageToSend) override;
  void DisableSending() override;
  void TriggerSendAsap() override;
  bool ShouldEcho() const override { return false; }
  OptionalMicroseconds GetLastReceiveTime() const override { return lastReceiveTime_; }

 protected:
  std::list<ProtocolMessage> GetReceivedMessagesImpl() override;
  void RunLoopImpl() override;
  virtual int Recv(void* buf, size_t bufsize, std::string* details) = 0;
  virtual void Send(void* buf, size_t bufsize) = 0;

 private:
  bool hasDataToSend_ = false;
  ProtocolMessage messageToSend_;

  PatternBits lastSentPattern_ = 0;

  Microseconds effectLastTxTime_ = 0;
  OptionalMicroseconds lastReceiveTime_;
};

}  // namespace jazzlights

#endif  // JL_NETWORK_NETWORK_H
