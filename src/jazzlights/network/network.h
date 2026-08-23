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
  X(INITIALIZING)            \
  X(CONNECTING)              \
  X(CONNECTED)               \
  X(CONNECTION_FAILED)

enum NetworkStatus {
#define X(s) s,
  ALL_NETWORK_STATUSES
#undef X
};

std::string NetworkStatusToString(NetworkStatus status);

std::string displayBitsAsBinary(PatternBits p);
std::string networkMessageToString(const NetworkMessage& message);

class Network {
 public:
  // Disallow copy and move.
  Network(const Network&) = delete;
  Network(Network&&) = delete;
  Network& operator=(const Network&) = delete;
  Network& operator=(Network&&) = delete;

  virtual ~Network() = default;

  // Set message to send during next send opportunity.
  virtual void setMessageToSend(const NetworkMessage& messageToSend) = 0;

  // Disables sending until the next call to setMessageToSend.
  virtual void disableSending() = 0;

  // Gets list of received messages since last call.
  std::list<NetworkMessage> getReceivedMessages();

  // Called once per primary runloop.
  void runLoop();

  // Get current network status.
  NetworkStatus status() const;

  // Get unique identifier for this network.
  NetworkId id() const { return id_; }

  // Request an immediate send.
  virtual void triggerSendAsap() = 0;

  // Returns this device's unique ID, often using its MAC address.
  virtual NetworkDeviceId getLocalDeviceId() const = 0;

  // The type of this network.
  virtual NetworkType type() const = 0;

  // Whether we should advertise patterns on this network if that's where we received them.
  virtual bool shouldEcho() const = 0;

  // Last time we received a message, or nullopt to indicate never.
  virtual OptionalMicroseconds getLastReceiveTime() const = 0;

  // Get a human-readable status string that can be displayed to the user. Not const to allow taking locks.
  virtual std::string getStatusStr() = 0;

 protected:
  Network() = default;
  // Perform any work necessary to switch to requested state.
  virtual NetworkStatus update(NetworkStatus status) = 0;
  // Gets list of received messages since last call.
  virtual std::list<NetworkMessage> getReceivedMessagesImpl() = 0;
  // Called once per primary runloop.
  virtual void runLoopImpl() = 0;
  NetworkStatus getStatus() const { return status_; }
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

  // Parse the UDP payload we use over IP networks into a NetworkMessage.
  bool ParseUdpPayload(uint8_t* udpPayload, size_t udpPayloadLength, const std::string& receiptDetails,
                       NetworkMessage* outMessage);

  // Write a NetworkMessage into a buffer that can be sent over UDP/IP.
  bool WriteUdpPayload(const NetworkMessage& messageToSend, uint8_t* udpPayload, size_t udpPayloadLength);

 private:
  void checkStatus();
  void reconnect();

  static NetworkId NextAvailableId();

  const NetworkId id_ = NextAvailableId();

  NetworkStatus status_ = INITIALIZING;

  Microseconds lastConnectionAttempt_ = 0;
  static constexpr Microseconds MinBackoffTimeout() { return 1000 * kMicrosecondsPerMillisecond; }
  static constexpr Microseconds MaxBackoffTimeout() { return 16000 * kMicrosecondsPerMillisecond; }
  Microseconds backoffTimeout_ = MinBackoffTimeout();
};

class UdpNetwork : public Network {
 public:
  void setMessageToSend(const NetworkMessage& messageToSend) override;
  void disableSending() override;
  void triggerSendAsap() override;
  bool shouldEcho() const override { return false; }
  OptionalMicroseconds getLastReceiveTime() const override { return lastReceiveTime_; }

 protected:
  std::list<NetworkMessage> getReceivedMessagesImpl() override;
  void runLoopImpl() override;
  virtual int recv(void* buf, size_t bufsize, std::string* details) = 0;
  virtual void send(void* buf, size_t bufsize) = 0;

 private:
  bool hasDataToSend_ = false;
  NetworkMessage messageToSend_;

  PatternBits lastSentPattern_ = 0;

  Microseconds effectLastTxTime_ = 0;
  OptionalMicroseconds lastReceiveTime_;
};

void writeUint32(uint8_t* data, uint32_t number);
void writeUint16(uint8_t* data, uint16_t number);
uint32_t readUint32(const uint8_t* data);
uint16_t readUint16(const uint8_t* data);

}  // namespace jazzlights

#endif  // JL_NETWORK_NETWORK_H
