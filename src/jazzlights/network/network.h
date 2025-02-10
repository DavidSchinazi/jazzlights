#ifndef JL_NETWORK_NETWORK_H
#define JL_NETWORK_NETWORK_H

#include <stdio.h>
#include <string.h>  // memcpy, size_t

#include <list>
#include <string>

#include "jazzlights/config.h"
#include "jazzlights/types.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

enum class NetworkType {
  kLeading,
  kBLE,
  kWiFi,
  kEthernet,
  kOther,
};

const char* NetworkTypeToString(NetworkType type);

// NetworkId 0 is a sentinel value that cannot be assigned to a network. It can mean that we generated the update
// instead of having received it.
using NetworkId = uint32_t;

class Network;

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

#define DEVICE_ID_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define DEVICE_ID_HEX(addr) (addr)(0), (addr)(1), (addr)(2), (addr)(3), (addr)(4), (addr)(5)

class NetworkDeviceId {
 public:
  explicit NetworkDeviceId() { memset(&data_[0], 0, kNetworkDeviceIdSize); }
  explicit NetworkDeviceId(const uint8_t* data) { memcpy(&data_[0], data, kNetworkDeviceIdSize); }
  NetworkDeviceId(const NetworkDeviceId& other) : NetworkDeviceId(&other.data_[0]) {}
  NetworkDeviceId& operator=(const NetworkDeviceId& other) {
    memcpy(&data_[0], &other.data_[0], kNetworkDeviceIdSize);
    return *this;
  }
  uint8_t operator()(uint8_t i) const { return data_[i]; }
  int compare(const NetworkDeviceId& other) const { return memcmp(&data_[0], &other.data_[0], kNetworkDeviceIdSize); }
  void writeTo(uint8_t* data) const { memcpy(data, &data_[0], kNetworkDeviceIdSize); }
  void readFrom(const uint8_t* data) { memcpy(&data_[0], data, kNetworkDeviceIdSize); }
  bool operator==(const NetworkDeviceId& other) const { return compare(other) == 0; }
  bool operator!=(const NetworkDeviceId& other) const { return compare(other) != 0; }
  bool operator<(const NetworkDeviceId& other) const { return compare(other) < 0; }
  bool operator<=(const NetworkDeviceId& other) const { return compare(other) <= 0; }
  bool operator>(const NetworkDeviceId& other) const { return compare(other) > 0; }
  bool operator>=(const NetworkDeviceId& other) const { return compare(other) >= 0; }
  std::string toString() const {
    char result[2 * 6 + 5 + 1] = {};
    snprintf(result, sizeof(result), DEVICE_ID_FMT, DEVICE_ID_HEX(*this));
    return result;
  }
  const uint8_t* data() const { return &data_[0]; }
  uint8_t* data() { return &data_[0]; }

  NetworkDeviceId PlusOne() const;

  static constexpr size_t size() { return kNetworkDeviceIdSize; }

 private:
  static constexpr size_t kNetworkDeviceIdSize = 6;
  uint8_t data_[kNetworkDeviceIdSize];
};

class Network;

struct NetworkMessage {
  NetworkDeviceId sender = NetworkDeviceId();
  NetworkDeviceId originator = NetworkDeviceId();
  Precedence precedence = 0;
  PatternBits currentPattern = 0;
  PatternBits nextPattern = 0;
  NumHops numHops = 0;
  // Times are sent over the wire as time since that event.
  Milliseconds currentPatternStartTime = 0;
  Milliseconds lastOriginationTime = 0;
  // Receipt values are not sent over the wire.
  // Note that, when sending, `receiptNetworkId` and `receiptNetworkType` represent
  // the network where our followed next hop is; or 0 / kLeading if we are leading.
  NetworkId receiptNetworkId = 0;
  NetworkType receiptNetworkType = NetworkType::kLeading;
  std::string receiptDetails;

#if JL_IS_CONFIG(CREATURE)
  int receiptRssi = -1000;
  Milliseconds receiptTime = -1;
  uint32_t creatureColor = 0;
#endif  // CREATURE

  bool isEqualExceptOriginationTime(const NetworkMessage& other) const {
    return sender == other.sender && originator == other.originator && precedence == other.precedence &&
           currentPattern == other.currentPattern && nextPattern == other.nextPattern && numHops == other.numHops &&
           currentPatternStartTime == other.currentPatternStartTime && receiptNetworkId == other.receiptNetworkId &&
           receiptNetworkType == other.receiptNetworkType;
  }
  bool operator==(const NetworkMessage& other) const {
    return isEqualExceptOriginationTime(other) && lastOriginationTime == other.lastOriginationTime;
  }
  bool operator!=(const NetworkMessage& other) const { return !(*this == other); }
};

std::string displayBitsAsBinary(PatternBits p);
std::string networkMessageToString(const NetworkMessage& message, Milliseconds currentTime);

class Network {
 public:
  // Disallow copy and move.
  Network(const Network&) = delete;
  Network(Network&&) = delete;
  Network& operator=(const Network&) = delete;
  Network& operator=(Network&&) = delete;

  virtual ~Network() = default;

  // Set message to send during next send opportunity.
  virtual void setMessageToSend(const NetworkMessage& messageToSend, Milliseconds currentTime) = 0;

  // Disables sending until the next call to setMessageToSend.
  virtual void disableSending(Milliseconds currentTime) = 0;

  // Gets list of received messages since last call.
  std::list<NetworkMessage> getReceivedMessages(Milliseconds currentTime);

  // Called once per primary runloop.
  void runLoop(Milliseconds currentTime);

  // Get current network status.
  NetworkStatus status() const;

  // Get unique identifier for this network.
  NetworkId id() const { return id_; }

  // Request an immediate send.
  virtual void triggerSendAsap(Milliseconds currentTime) = 0;

  // Returns this device's unique ID, often using its MAC address.
  virtual NetworkDeviceId getLocalDeviceId() const = 0;

  // The type of this network.
  virtual NetworkType type() const = 0;

  // Whether we should advertise patterns on this network if that's where we received them.
  virtual bool shouldEcho() const = 0;

  // Last time we received a message, or -1 to indicate never.
  virtual Milliseconds getLastReceiveTime() const = 0;

  // Get a human-readable status string that can be displayed to the user. Not const to allow taking locks.
  virtual std::string getStatusStr(Milliseconds currentTime) = 0;

 protected:
  Network() = default;
  // Perform any work necessary to switch to requested state.
  virtual NetworkStatus update(NetworkStatus status, Milliseconds currentTime) = 0;
  // Gets list of received messages since last call.
  virtual std::list<NetworkMessage> getReceivedMessagesImpl(Milliseconds currentTime) = 0;
  // Called once per primary runloop.
  virtual void runLoopImpl(Milliseconds currentTime) = 0;
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
                       Milliseconds currentTime, NetworkMessage* outMessage);

  // Write a NetworkMessage into a buffer that can be sent over UDP/IP.
  bool WriteUdpPayload(const NetworkMessage& messageToSend, uint8_t* udpPayload, size_t udpPayloadLength,
                       Milliseconds currentTime);

 private:
  void checkStatus(Milliseconds currentTime);
  void reconnect(Milliseconds currentTime);

  static NetworkId NextAvailableId();

  const NetworkId id_ = NextAvailableId();

  NetworkStatus status_ = INITIALIZING;

  Milliseconds lastConnectionAttempt_ = 0;
  static constexpr Milliseconds MinBackoffTimeout() { return 1000; }
  static constexpr Milliseconds MaxBackoffTimeout() { return 16000; }
  Milliseconds backoffTimeout_ = MinBackoffTimeout();
};

class UdpNetwork : public Network {
 public:
  void setMessageToSend(const NetworkMessage& messageToSend, Milliseconds currentTime) override;
  void disableSending(Milliseconds currentTime) override;
  void triggerSendAsap(Milliseconds currentTime) override;
  bool shouldEcho() const override { return false; }
  Milliseconds getLastReceiveTime() const override { return lastReceiveTime_; }

 protected:
  std::list<NetworkMessage> getReceivedMessagesImpl(Milliseconds currentTime) override;
  void runLoopImpl(Milliseconds currentTime) override;
  virtual int recv(void* buf, size_t bufsize, std::string* details) = 0;
  virtual void send(void* buf, size_t bufsize) = 0;

 private:
  bool hasDataToSend_ = false;
  NetworkMessage messageToSend_;

  PatternBits lastSentPattern_ = 0;

  Milliseconds effectLastTxTime_ = 0;
  Milliseconds lastReceiveTime_ = -1;
};

void writeUint32(uint8_t* data, uint32_t number);
void writeUint16(uint8_t* data, uint16_t number);
uint32_t readUint32(const uint8_t* data);
uint16_t readUint16(const uint8_t* data);

class NetworkReader {
 public:
  explicit NetworkReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

  bool ReadUint8(uint8_t* out) {
    if (sizeof(*out) > size_ || pos_ > size_ - sizeof(*out)) { return false; }
    *out = data_[pos_];
    pos_ += sizeof(*out);
    return true;
  }

  bool ReadUint16(uint16_t* out) {
    if (sizeof(*out) > size_ || pos_ > size_ - sizeof(*out)) { return false; }
    *out = (data_[pos_] << 8) | (data_[pos_ + 1]);
    pos_ += sizeof(*out);
    return true;
  }

  bool ReadUint32(uint32_t* out) {
    if (sizeof(*out) > size_ || pos_ > size_ - sizeof(*out)) { return false; }
    *out = (data_[pos_] << 24) | (data_[pos_ + 1] << 16) | (data_[pos_ + 2] << 8) | (data_[pos_ + 3]);
    pos_ += sizeof(*out);
    return true;
  }

  bool ReadNetworkDeviceId(NetworkDeviceId* out) {
    if (NetworkDeviceId::size() > size_ || pos_ > size_ - NetworkDeviceId::size()) { return false; }
    out->readFrom(&data_[pos_]);
    pos_ += NetworkDeviceId::size();
    return true;
  }

 private:
  const uint8_t* data_;
  const size_t size_;
  size_t pos_ = 0;
};

class NetworkWriter {
 public:
  explicit NetworkWriter(uint8_t* data, size_t size) : data_(data), size_(size) {}

  bool WriteUint8(uint8_t in) {
    if (sizeof(in) > size_ || pos_ > size_ - sizeof(in)) { return false; }
    data_[pos_] = in;
    pos_ += sizeof(in);
    return true;
  }

  bool WriteUint16(uint16_t in) {
    if (sizeof(in) > size_ || pos_ > size_ - sizeof(in)) { return false; }
    data_[pos_] = static_cast<uint8_t>((in & 0xFF00) >> 8);
    data_[pos_ + 1] = static_cast<uint8_t>((in & 0x00FF));
    pos_ += sizeof(in);
    return true;
  }

  bool WriteUint32(uint32_t in) {
    if (sizeof(in) > size_ || pos_ > size_ - sizeof(in)) { return false; }
    data_[pos_] = static_cast<uint8_t>((in & 0xFF000000) >> 24);
    data_[pos_ + 1] = static_cast<uint8_t>((in & 0x00FF0000) >> 16);
    data_[pos_ + 2] = static_cast<uint8_t>((in & 0x0000FF00) >> 8);
    data_[pos_ + 3] = static_cast<uint8_t>((in & 0x000000FF));
    pos_ += sizeof(in);
    return true;
  }

  bool WriteNetworkDeviceId(const NetworkDeviceId& in) {
    if (NetworkDeviceId::size() > size_ || pos_ > size_ - NetworkDeviceId::size()) { return false; }
    in.writeTo(&data_[pos_]);
    pos_ += NetworkDeviceId::size();
    return true;
  }

 private:
  uint8_t* data_;
  const size_t size_;
  size_t pos_ = 0;
};

}  // namespace jazzlights

#endif  // JL_NETWORK_NETWORK_H
