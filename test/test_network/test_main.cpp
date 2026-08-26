#include <unity.h>

#include <optional>

#include "jazzlights/network/network.h"
#include "jazzlights/protocol/reader.h"
#include "jazzlights/protocol/wire.h"
#include "jazzlights/protocol/writer.h"

namespace jazzlights {

void TestNetworkReader() {
  uint8_t buffer[7] = {1, 2, 3, 4, 5, 6, 7};
  ProtocolReader reader(buffer, sizeof(buffer));
  uint8_t u8;
  TEST_ASSERT(reader.ReadUint8(&u8));
  TEST_ASSERT_EQUAL(0x01, u8);
  uint16_t u16;
  TEST_ASSERT(reader.ReadUint16(&u16));
  TEST_ASSERT_EQUAL(0x0203, u16);
  uint32_t u32;
  TEST_ASSERT(reader.ReadUint32(&u32));
  TEST_ASSERT_EQUAL(0x04050607, u32);
  TEST_ASSERT_FALSE(reader.ReadUint8(&u8));
}
void TestNetworkWriter() {
  uint8_t buffer[7] = {};
  ProtocolWriter writer(buffer, sizeof(buffer));
  uint8_t u8 = 1;
  TEST_ASSERT(writer.WriteUint8(u8));
  TEST_ASSERT_EQUAL(0x01, buffer[0]);
  uint16_t u16 = 0x0203;
  TEST_ASSERT(writer.WriteUint16(u16));
  TEST_ASSERT_EQUAL(0x02, buffer[1]);
  TEST_ASSERT_EQUAL(0x03, buffer[2]);
  uint32_t u32 = 0x04050607;
  TEST_ASSERT(writer.WriteUint32(u32));
  TEST_ASSERT_EQUAL(0x04, buffer[3]);
  TEST_ASSERT_EQUAL(0x05, buffer[4]);
  TEST_ASSERT_EQUAL(0x06, buffer[5]);
  TEST_ASSERT_EQUAL(0x07, buffer[6]);
  TEST_ASSERT_FALSE(writer.WriteUint8(u8));
}

void TestNetworkInt32() {
  int32_t values[] = {0, -1, 2147483647, -2147483648};
  uint8_t expectedBytes[][4] = {
      {0x00, 0x00, 0x00, 0x00},
      {0xFF, 0xFF, 0xFF, 0xFF},
      {0x7F, 0xFF, 0xFF, 0xFF},
      {0x80, 0x00, 0x00, 0x00},
  };

  for (size_t i = 0; i < 4; ++i) {
    uint8_t buffer[4];
    ProtocolWriter writer(buffer, sizeof(buffer));
    TEST_ASSERT(writer.WriteInt32(values[i]));
    for (size_t j = 0; j < 4; ++j) { TEST_ASSERT_EQUAL_HEX8(expectedBytes[i][j], buffer[j]); }

    ProtocolReader reader(buffer, sizeof(buffer));
    int32_t out;
    TEST_ASSERT(reader.ReadInt32(&out));
    TEST_ASSERT_EQUAL_INT32(values[i], out);
  }
}

namespace {

// Arbitrary point in time used as both send and receipt time so that the durations on the wire round-trip exactly.
constexpr Microseconds kSendTime = 1000 * kMicrosecondsPerSecond;

ProtocolMessage MakeMessageToSend() {
  uint8_t originatorBytes[6] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab};
  uint8_t senderBytes[6] = {0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54};
  ProtocolMessage message;
  message.originator = NetworkDeviceId(originatorBytes);
  message.sender = NetworkDeviceId(senderBytes);
  message.precedence = 0x1234;
  message.currentPattern = 0x89abcdef;
  message.nextPattern = 0x76543210;
  message.numHops = 3;
  message.currentPatternStartTime = kSendTime - 4000 * kMicrosecondsPerMillisecond;
  message.lastOriginationTime = kSendTime - 250 * kMicrosecondsPerMillisecond;
  return message;
}

// Compares the fields that are sent over the wire. ProtocolMessage::operator==() is not sufficient here because it
// ignores orrerySceneId, and because it compares the sender which is not sent over the wire over BLE.
void AssertWireFieldsEqual(const ProtocolMessage& expected, const ProtocolMessage& actual, bool isBle) {
  TEST_ASSERT(expected.originator == actual.originator);
  TEST_ASSERT_EQUAL(expected.precedence, actual.precedence);
  TEST_ASSERT_EQUAL(expected.currentPattern, actual.currentPattern);
  TEST_ASSERT_EQUAL(expected.nextPattern, actual.nextPattern);
  TEST_ASSERT_EQUAL(expected.numHops, actual.numHops);
  TEST_ASSERT_EQUAL(expected.currentPatternStartTime, actual.currentPatternStartTime);
  TEST_ASSERT_EQUAL(expected.lastOriginationTime, actual.lastOriginationTime);
  if (isBle) {
    // The sender is only sent over the wire over UDP, and the extension byte is only sent over BLE.
    TEST_ASSERT_EQUAL(expected.orrerySceneId.has_value(), actual.orrerySceneId.has_value());
    if (expected.orrerySceneId) { TEST_ASSERT_EQUAL(*expected.orrerySceneId, *actual.orrerySceneId); }
  } else {
    TEST_ASSERT(expected.sender == actual.sender);
    TEST_ASSERT_FALSE(actual.orrerySceneId.has_value());
  }
}

// The wire representation of the durations that MakeMessageToSend() produces when sent at kSendTime.
constexpr uint8_t kOriginationTimeDeltaBytes[2] = {0x00, 0xfa};  // 250ms.
constexpr uint8_t kPatternTimeDeltaBytes[2] = {0x0f, 0xa0};      // 4000ms.

}  // namespace

void TestProtocolMessageRoundTrip() {
  const ProtocolMessage sent = MakeMessageToSend();
  uint8_t payload[kMaxBleProtocolPayloadLength] = {};
  const size_t payloadLength = WriteProtocolMessage(sent, /*isBle=*/true, payload, sizeof(payload), kSendTime);
  TEST_ASSERT_EQUAL(kMinBleProtocolPayloadLength, payloadLength);

  const std::optional<ProtocolMessage> received =
      ParseProtocolMessage(payload, payloadLength, /*isBle=*/true, kSendTime);
  TEST_ASSERT(received.has_value());
  AssertWireFieldsEqual(sent, *received, /*isBle=*/true);
}

void TestProtocolMessageRoundTripWithOrreryScene() {
  ProtocolMessage sent = MakeMessageToSend();
  sent.orrerySceneId = static_cast<OrrerySceneId>(OrreryScene::kSilly);
  uint8_t payload[kMaxBleProtocolPayloadLength] = {};
  const size_t payloadLength = WriteProtocolMessage(sent, /*isBle=*/true, payload, sizeof(payload), kSendTime);
  TEST_ASSERT_EQUAL(kMinBleProtocolPayloadLength + 2, payloadLength);

  const std::optional<ProtocolMessage> received =
      ParseProtocolMessage(payload, payloadLength, /*isBle=*/true, kSendTime);
  TEST_ASSERT(received.has_value());
  AssertWireFieldsEqual(sent, *received, /*isBle=*/true);
}

void TestProtocolMessageParseRejectsShortPayload() {
  const ProtocolMessage sent = MakeMessageToSend();
  uint8_t payload[kMaxBleProtocolPayloadLength] = {};
  const size_t payloadLength = WriteProtocolMessage(sent, /*isBle=*/true, payload, sizeof(payload), kSendTime);
  TEST_ASSERT_GREATER_THAN(0, payloadLength);
  TEST_ASSERT_FALSE(ParseProtocolMessage(payload, payloadLength - 1, /*isBle=*/true, kSendTime).has_value());
}

void TestProtocolMessageWriteRejectsSmallBuffer() {
  const ProtocolMessage sent = MakeMessageToSend();
  uint8_t payload[kMinBleProtocolPayloadLength - 1] = {};
  TEST_ASSERT_EQUAL(0, WriteProtocolMessage(sent, /*isBle=*/true, payload, sizeof(payload), kSendTime));
}

void TestBleProtocolMessageWireBytes() {
  const ProtocolMessage sent = MakeMessageToSend();
  uint8_t payload[kMaxBleProtocolPayloadLength] = {};
  const size_t payloadLength = WriteProtocolMessage(sent, /*isBle=*/true, payload, sizeof(payload), kSendTime);
  TEST_ASSERT_EQUAL(kMinBleProtocolPayloadLength, payloadLength);
  const uint8_t expected[kMinBleProtocolPayloadLength] = {
      0x01,
      0x23,
      0x45,
      0x67,
      0x89,
      0xab,  // originator
      0x12,
      0x34,  // precedence
      0x03,  // numHops
      kOriginationTimeDeltaBytes[0],
      kOriginationTimeDeltaBytes[1],  // originationTime
      0x89,
      0xab,
      0xcd,
      0xef,  // currentPattern
      0x76,
      0x54,
      0x32,
      0x10,  // nextPattern
      kPatternTimeDeltaBytes[0],
      kPatternTimeDeltaBytes[1],  // patternTime
  };
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, payload, sizeof(expected));
}

void TestUdpProtocolMessageWireBytes() {
  const ProtocolMessage sent = MakeMessageToSend();
  uint8_t payload[kUdpProtocolPayloadLength] = {};
  const size_t payloadLength = WriteProtocolMessage(sent, /*isBle=*/false, payload, sizeof(payload), kSendTime);
  TEST_ASSERT_EQUAL(kUdpProtocolPayloadLength, payloadLength);
  const uint8_t expected[kUdpProtocolPayloadLength] = {
      0x10,  // version
      0x01,
      0x23,
      0x45,
      0x67,
      0x89,
      0xab,  // originator
      0xfe,
      0xdc,
      0xba,
      0x98,
      0x76,
      0x54,  // sender
      0x12,
      0x34,  // precedence
      0x03,  // numHops
      kOriginationTimeDeltaBytes[0],
      kOriginationTimeDeltaBytes[1],  // originationTime
      0x89,
      0xab,
      0xcd,
      0xef,  // currentPattern
      0x76,
      0x54,
      0x32,
      0x10,  // nextPattern
      kPatternTimeDeltaBytes[0],
      kPatternTimeDeltaBytes[1],  // patternTime
  };
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, payload, sizeof(expected));
}

void TestUdpProtocolMessageRoundTrip() {
  const ProtocolMessage sent = MakeMessageToSend();
  uint8_t payload[kUdpProtocolPayloadLength] = {};
  const size_t payloadLength = WriteProtocolMessage(sent, /*isBle=*/false, payload, sizeof(payload), kSendTime);
  TEST_ASSERT_EQUAL(kUdpProtocolPayloadLength, payloadLength);

  const std::optional<ProtocolMessage> received =
      ParseProtocolMessage(payload, payloadLength, /*isBle=*/false, kSendTime);
  TEST_ASSERT(received.has_value());
  AssertWireFieldsEqual(sent, *received, /*isBle=*/false);
}

void TestUdpProtocolMessageParseRejectsBadVersion() {
  const ProtocolMessage sent = MakeMessageToSend();
  uint8_t payload[kUdpProtocolPayloadLength] = {};
  const size_t payloadLength = WriteProtocolMessage(sent, /*isBle=*/false, payload, sizeof(payload), kSendTime);
  TEST_ASSERT_EQUAL(kUdpProtocolPayloadLength, payloadLength);

  payload[0] = 0x20;
  TEST_ASSERT_FALSE(ParseProtocolMessage(payload, payloadLength, /*isBle=*/false, kSendTime).has_value());
  // Only the high nibble of the version byte is checked.
  payload[0] = 0x1f;
  TEST_ASSERT(ParseProtocolMessage(payload, payloadLength, /*isBle=*/false, kSendTime).has_value());
}

void TestUdpProtocolMessageParseRejectsShortPayload() {
  const ProtocolMessage sent = MakeMessageToSend();
  uint8_t payload[kUdpProtocolPayloadLength] = {};
  const size_t payloadLength = WriteProtocolMessage(sent, /*isBle=*/false, payload, sizeof(payload), kSendTime);
  TEST_ASSERT_EQUAL(kUdpProtocolPayloadLength, payloadLength);
  TEST_ASSERT_FALSE(ParseProtocolMessage(payload, payloadLength - 1, /*isBle=*/false, kSendTime).has_value());

  // A payload that is long enough over BLE is too short over UDP.
  uint8_t blePayload[kMaxBleProtocolPayloadLength] = {};
  const size_t bleLength = WriteProtocolMessage(sent, /*isBle=*/true, blePayload, sizeof(blePayload), kSendTime);
  TEST_ASSERT_EQUAL(kMinBleProtocolPayloadLength, bleLength);
  TEST_ASSERT_FALSE(ParseProtocolMessage(blePayload, bleLength, /*isBle=*/false, kSendTime).has_value());
}

void TestUdpProtocolMessageWriteRejectsSmallBuffer() {
  const ProtocolMessage sent = MakeMessageToSend();
  uint8_t payload[kUdpProtocolPayloadLength - 1] = {};
  TEST_ASSERT_EQUAL(0, WriteProtocolMessage(sent, /*isBle=*/false, payload, sizeof(payload), kSendTime));
}

void TestUdpProtocolMessageParseIgnoresTrailingBytes() {
  const ProtocolMessage sent = MakeMessageToSend();
  uint8_t payload[kUdpProtocolPayloadLength + 4] = {};
  const size_t payloadLength =
      WriteProtocolMessage(sent, /*isBle=*/false, payload, kUdpProtocolPayloadLength, kSendTime);
  TEST_ASSERT_EQUAL(kUdpProtocolPayloadLength, payloadLength);
  for (size_t i = kUdpProtocolPayloadLength; i < sizeof(payload); i++) { payload[i] = 0xA5; }

  const std::optional<ProtocolMessage> received =
      ParseProtocolMessage(payload, sizeof(payload), /*isBle=*/false, kSendTime);
  TEST_ASSERT(received.has_value());
  AssertWireFieldsEqual(sent, *received, /*isBle=*/false);
}

void TestUdpProtocolMessageIgnoresOrreryScene() {
  ProtocolMessage sent = MakeMessageToSend();
  sent.orrerySceneId = static_cast<OrrerySceneId>(OrreryScene::kSilly);
  uint8_t payload[kUdpProtocolPayloadLength + 2] = {};
  const size_t payloadLength = WriteProtocolMessage(sent, /*isBle=*/false, payload, sizeof(payload), kSendTime);
  // The orrery scene is not sent over UDP, so the payload length is unchanged.
  TEST_ASSERT_EQUAL(kUdpProtocolPayloadLength, payloadLength);

  const std::optional<ProtocolMessage> received =
      ParseProtocolMessage(payload, payloadLength, /*isBle=*/false, kSendTime);
  TEST_ASSERT(received.has_value());
  TEST_ASSERT_FALSE(received->orrerySceneId.has_value());
}

void RunUnityTests() {
  UNITY_BEGIN();
  RUN_TEST(TestNetworkReader);
  RUN_TEST(TestNetworkWriter);
  RUN_TEST(TestNetworkInt32);
  RUN_TEST(TestProtocolMessageRoundTrip);
  RUN_TEST(TestProtocolMessageRoundTripWithOrreryScene);
  RUN_TEST(TestProtocolMessageParseRejectsShortPayload);
  RUN_TEST(TestProtocolMessageWriteRejectsSmallBuffer);
  RUN_TEST(TestBleProtocolMessageWireBytes);
  RUN_TEST(TestUdpProtocolMessageWireBytes);
  RUN_TEST(TestUdpProtocolMessageRoundTrip);
  RUN_TEST(TestUdpProtocolMessageParseRejectsBadVersion);
  RUN_TEST(TestUdpProtocolMessageParseRejectsShortPayload);
  RUN_TEST(TestUdpProtocolMessageWriteRejectsSmallBuffer);
  RUN_TEST(TestUdpProtocolMessageParseIgnoresTrailingBytes);
  RUN_TEST(TestUdpProtocolMessageIgnoresOrreryScene);
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
