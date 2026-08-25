#include <unity.h>

#include <list>
#include <optional>
#include <string>
#include <vector>

#include "jazzlights/network/manager.h"

namespace jazzlights {
namespace {

// A transport that records everything NetworkManager asks of it and hands back queued received messages.
class FakeNetwork : public Network {
 public:
  explicit FakeNetwork(NetworkType type, bool shouldEcho) : type_(type), shouldEcho_(shouldEcho) {}

  void SetLocalDeviceId(NetworkDeviceId localDeviceId) { localDeviceId_ = localDeviceId; }
  void QueueReceivedMessage(const ProtocolMessage& message) { messagesToReceive_.push_back(message); }

  void SetMessageToSend(const ProtocolMessage& messageToSend) override {
    sentMessages.push_back(messageToSend);
    sendingDisabled = false;
  }
  void DisableSending() override { sendingDisabled = true; }
  void TriggerSendAsap() override { sendAsapCount++; }
  NetworkDeviceId GetLocalDeviceId() const override { return localDeviceId_; }
  NetworkType Type() const override { return type_; }
  bool ShouldEcho() const override { return shouldEcho_; }
  OptionalMicroseconds GetLastReceiveTime() const override { return std::nullopt; }
  std::string GetStatusStr() override { return "fake"; }

  std::vector<ProtocolMessage> sentMessages;
  bool sendingDisabled = false;
  int sendAsapCount = 0;
  int runLoopCount = 0;

 protected:
  NetworkStatus Update(NetworkStatus /*status*/) override { return kConnected; }
  std::list<ProtocolMessage> GetReceivedMessagesImpl() override {
    std::list<ProtocolMessage> messages;
    messages.swap(messagesToReceive_);
    return messages;
  }
  void RunLoopImpl() override { runLoopCount++; }

 private:
  const NetworkType type_;
  const bool shouldEcho_;
  NetworkDeviceId localDeviceId_ = NetworkDeviceId();
  std::list<ProtocolMessage> messagesToReceive_;
};

NetworkDeviceId MakeDeviceId(uint8_t lastByte) {
  uint8_t bytes[6] = {0, 0, 0, 0, 0, lastByte};
  return NetworkDeviceId(bytes);
}

// A non-reserved pattern (lowest four bits are non-zero).
constexpr PatternBits kPatternA = 0x12345671;

ProtocolMessage MakeMessage(NetworkId receiptNetworkId) {
  ProtocolMessage message;
  message.originator = MakeDeviceId(0x11);
  message.sender = MakeDeviceId(0x11);
  message.precedence = 1000;
  message.currentPattern = kPatternA;
  message.receiptNetworkId = receiptNetworkId;
  message.receiptNetworkType = NetworkType::kWiFi;
  return message;
}

}  // namespace

void TestHasNetworks() {
  NetworkManager manager;
  TEST_ASSERT_FALSE(manager.HasNetworks());
  FakeNetwork network(NetworkType::kWiFi, /*shouldEcho=*/false);
  manager.Connect(&network);
  TEST_ASSERT_TRUE(manager.HasNetworks());
}

void TestSendFanOut() {
  NetworkManager manager;
  FakeNetwork wifi(NetworkType::kWiFi, /*shouldEcho=*/false);
  FakeNetwork ble(NetworkType::kBLE, /*shouldEcho=*/true);
  manager.Connect(&wifi);
  manager.Connect(&ble);

  // A message we originated ourselves goes out on every transport.
  const ProtocolMessage message = MakeMessage(/*receiptNetworkId=*/0);
  manager.SetMessageToSend(message);
  TEST_ASSERT_EQUAL_size_t(1, wifi.sentMessages.size());
  TEST_ASSERT_EQUAL_size_t(1, ble.sentMessages.size());
  TEST_ASSERT_TRUE(message == wifi.sentMessages[0]);
  TEST_ASSERT_TRUE(message == ble.sentMessages[0]);
  TEST_ASSERT_FALSE(wifi.sendingDisabled);
  TEST_ASSERT_FALSE(ble.sendingDisabled);

  // Sending asap is opt-in.
  TEST_ASSERT_EQUAL_INT(0, wifi.sendAsapCount);
  TEST_ASSERT_EQUAL_INT(0, ble.sendAsapCount);
  manager.SetMessageToSend(message, /*sendAsap=*/true);
  TEST_ASSERT_EQUAL_INT(1, wifi.sendAsapCount);
  TEST_ASSERT_EQUAL_INT(1, ble.sendAsapCount);
}

void TestEchoSuppression() {
  NetworkManager manager;
  FakeNetwork wifi(NetworkType::kWiFi, /*shouldEcho=*/false);
  FakeNetwork ethernet(NetworkType::kEthernet, /*shouldEcho=*/false);
  manager.Connect(&wifi);
  manager.Connect(&ethernet);

  // A message received on Wi-Fi is not echoed back to Wi-Fi, but is still sent on Ethernet.
  manager.SetMessageToSend(MakeMessage(/*receiptNetworkId=*/wifi.id()));
  TEST_ASSERT_TRUE(wifi.sentMessages.empty());
  TEST_ASSERT_TRUE(wifi.sendingDisabled);
  TEST_ASSERT_EQUAL_size_t(1, ethernet.sentMessages.size());
  TEST_ASSERT_FALSE(ethernet.sendingDisabled);
}

void TestEchoingNetworkStillSends() {
  NetworkManager manager;
  FakeNetwork ble(NetworkType::kBLE, /*shouldEcho=*/true);
  manager.Connect(&ble);

  // BLE echoes, so it advertises even what it just received.
  manager.SetMessageToSend(MakeMessage(/*receiptNetworkId=*/ble.id()));
  TEST_ASSERT_EQUAL_size_t(1, ble.sentMessages.size());
  TEST_ASSERT_FALSE(ble.sendingDisabled);
}

void TestNothingToSendIsNoOp() {
  NetworkManager manager;
  FakeNetwork wifi(NetworkType::kWiFi, /*shouldEcho=*/false);
  manager.Connect(&wifi);

  manager.SetMessageToSend(std::nullopt, /*sendAsap=*/true);
  TEST_ASSERT_TRUE(wifi.sentMessages.empty());
  TEST_ASSERT_FALSE(wifi.sendingDisabled);
  TEST_ASSERT_EQUAL_INT(0, wifi.sendAsapCount);
}

void TestReceiveAggregation() {
  NetworkManager manager;
  FakeNetwork wifi(NetworkType::kWiFi, /*shouldEcho=*/false);
  FakeNetwork ble(NetworkType::kBLE, /*shouldEcho=*/true);
  manager.Connect(&wifi);
  manager.Connect(&ble);

  wifi.QueueReceivedMessage(MakeMessage(/*receiptNetworkId=*/0));
  wifi.QueueReceivedMessage(MakeMessage(/*receiptNetworkId=*/0));
  ble.QueueReceivedMessage(MakeMessage(/*receiptNetworkId=*/0));

  // All three messages come back from a single call, each stamped with the network it arrived on.
  std::list<ProtocolMessage> receivedMessages = manager.GetReceivedMessages();
  TEST_ASSERT_EQUAL_size_t(3, receivedMessages.size());
  size_t numFromWiFi = 0;
  size_t numFromBle = 0;
  for (const ProtocolMessage& message : receivedMessages) {
    if (message.receiptNetworkId == wifi.id()) {
      TEST_ASSERT_TRUE(message.receiptNetworkType == NetworkType::kWiFi);
      numFromWiFi++;
    } else if (message.receiptNetworkId == ble.id()) {
      TEST_ASSERT_TRUE(message.receiptNetworkType == NetworkType::kBLE);
      numFromBle++;
    }
  }
  TEST_ASSERT_EQUAL_size_t(2, numFromWiFi);
  TEST_ASSERT_EQUAL_size_t(1, numFromBle);

  // The messages are consumed.
  TEST_ASSERT_TRUE(manager.GetReceivedMessages().empty());
}

void TestRunLoop() {
  NetworkManager manager;
  FakeNetwork wifi(NetworkType::kWiFi, /*shouldEcho=*/false);
  FakeNetwork ble(NetworkType::kBLE, /*shouldEcho=*/true);
  manager.Connect(&wifi);
  manager.Connect(&ble);

  manager.RunLoop();
  TEST_ASSERT_EQUAL_INT(1, wifi.runLoopCount);
  TEST_ASSERT_EQUAL_INT(1, ble.runLoopCount);
}

void TestLocalDeviceId() {
  NetworkManager manager;
  FakeNetwork withoutId(NetworkType::kWiFi, /*shouldEcho=*/false);
  FakeNetwork withId(NetworkType::kBLE, /*shouldEcho=*/true);
  manager.Connect(&withoutId);
  manager.Connect(&withId);

  // Transports without a device ID are skipped in favor of the first one that has one.
  TEST_ASSERT_TRUE(NetworkDeviceId() == manager.GetLocalDeviceId());
  withId.SetLocalDeviceId(MakeDeviceId(0x42));
  TEST_ASSERT_TRUE(MakeDeviceId(0x42) == manager.GetLocalDeviceId());

  // The first transport with an ID wins.
  withoutId.SetLocalDeviceId(MakeDeviceId(0x43));
  TEST_ASSERT_TRUE(MakeDeviceId(0x43) == manager.GetLocalDeviceId());
}

void TestNoNetworks() {
  NetworkManager manager;
  TEST_ASSERT_FALSE(manager.HasNetworks());
  TEST_ASSERT_TRUE(NetworkDeviceId() == manager.GetLocalDeviceId());
  TEST_ASSERT_TRUE(manager.GetReceivedMessages().empty());
  manager.SetMessageToSend(MakeMessage(/*receiptNetworkId=*/0), /*sendAsap=*/true);
  manager.RunLoop();
}

void RunUnityTests() {
  UNITY_BEGIN();
  RUN_TEST(TestHasNetworks);
  RUN_TEST(TestSendFanOut);
  RUN_TEST(TestEchoSuppression);
  RUN_TEST(TestEchoingNetworkStillSends);
  RUN_TEST(TestNothingToSendIsNoOp);
  RUN_TEST(TestReceiveAggregation);
  RUN_TEST(TestRunLoop);
  RUN_TEST(TestLocalDeviceId);
  RUN_TEST(TestNoNetworks);
  UNITY_END();
}

}  // namespace jazzlights

void setUp() {}

void tearDown() {}

#ifdef ESP32

void setup() { jazzlights::RunUnityTests(); }

void loop() {}

#else  // ESP32

int main(int /*argc*/, char** /*argv*/) {
  jazzlights::RunUnityTests();
  return 0;
}

#endif  // ESP32
