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
  ProtocolMessage message;
  message.originator = NetworkDeviceId(originatorBytes);
  message.precedence = 0x1234;
  message.currentPattern = 0x89abcdef;
  message.nextPattern = 0x76543210;
  message.numHops = 3;
  message.currentPatternStartTime = kSendTime - 4000 * kMicrosecondsPerMillisecond;
  message.lastOriginationTime = kSendTime - 250 * kMicrosecondsPerMillisecond;
  return message;
}

// Compares the fields that are sent over the wire. ProtocolMessage::operator==() is not sufficient here because it
// ignores orrerySceneId, and because it compares the sender which is not sent over the wire.
void AssertWireFieldsEqual(const ProtocolMessage& expected, const ProtocolMessage& actual) {
  TEST_ASSERT(expected.originator == actual.originator);
  TEST_ASSERT_EQUAL(expected.precedence, actual.precedence);
  TEST_ASSERT_EQUAL(expected.currentPattern, actual.currentPattern);
  TEST_ASSERT_EQUAL(expected.nextPattern, actual.nextPattern);
  TEST_ASSERT_EQUAL(expected.numHops, actual.numHops);
  TEST_ASSERT_EQUAL(expected.currentPatternStartTime, actual.currentPatternStartTime);
  TEST_ASSERT_EQUAL(expected.lastOriginationTime, actual.lastOriginationTime);
  TEST_ASSERT_EQUAL(expected.orrerySceneId.has_value(), actual.orrerySceneId.has_value());
  if (expected.orrerySceneId) { TEST_ASSERT_EQUAL(*expected.orrerySceneId, *actual.orrerySceneId); }
}

}  // namespace

void TestProtocolMessageRoundTrip() {
  const ProtocolMessage sent = MakeMessageToSend();
  uint8_t payload[kMaxProtocolPayloadLength] = {};
  const size_t payloadLength = WriteProtocolMessage(sent, payload, sizeof(payload), kSendTime);
  TEST_ASSERT_EQUAL(kMinProtocolPayloadLength, payloadLength);

  const std::optional<ProtocolMessage> received = ParseProtocolMessage(payload, payloadLength, kSendTime);
  TEST_ASSERT(received.has_value());
  AssertWireFieldsEqual(sent, *received);
}

void TestProtocolMessageRoundTripWithOrreryScene() {
  ProtocolMessage sent = MakeMessageToSend();
  sent.orrerySceneId = static_cast<OrrerySceneId>(OrreryScene::kSilly);
  uint8_t payload[kMaxProtocolPayloadLength] = {};
  const size_t payloadLength = WriteProtocolMessage(sent, payload, sizeof(payload), kSendTime);
  TEST_ASSERT_EQUAL(kMinProtocolPayloadLength + 2, payloadLength);

  const std::optional<ProtocolMessage> received = ParseProtocolMessage(payload, payloadLength, kSendTime);
  TEST_ASSERT(received.has_value());
  AssertWireFieldsEqual(sent, *received);
}

void TestProtocolMessageParseRejectsShortPayload() {
  const ProtocolMessage sent = MakeMessageToSend();
  uint8_t payload[kMaxProtocolPayloadLength] = {};
  const size_t payloadLength = WriteProtocolMessage(sent, payload, sizeof(payload), kSendTime);
  TEST_ASSERT_GREATER_THAN(0, payloadLength);
  TEST_ASSERT_FALSE(ParseProtocolMessage(payload, payloadLength - 1, kSendTime).has_value());
}

void TestProtocolMessageWriteRejectsSmallBuffer() {
  const ProtocolMessage sent = MakeMessageToSend();
  uint8_t payload[kMinProtocolPayloadLength - 1] = {};
  TEST_ASSERT_EQUAL(0, WriteProtocolMessage(sent, payload, sizeof(payload), kSendTime));
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
