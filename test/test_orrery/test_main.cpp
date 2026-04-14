#include <unity.h>

#include "jazzlights/network/network.h"
#include "jazzlights/orrery_common.h"

namespace jazzlights {

void test_orrery_message_serialization() {
  OrreryMessage msg1;
  msg1.leaderBootId = 0x12345678;
  msg1.leaderSequenceNumber = 0x87654321;
  msg1.speed = 1000;
  msg1.position = 5000;
  msg1.calibration = 0;
  msg1.ledPattern = 0xABCDEF;
  msg1.ledBrightness = 200;
  msg1.ledPrecedence = 10000;

  uint8_t buffer[64];
  NetworkWriter writer(buffer, sizeof(buffer));
  TEST_ASSERT(WriteOrreryMessage(OrreryMessageType::LeaderCommand, msg1, writer));

  NetworkReader reader(buffer, writer.LengthWritten());
  OrreryMessageType type;
  OrreryMessage msg2;
  TEST_ASSERT(ReadOrreryMessage(reader, &type, &msg2));

  TEST_ASSERT_EQUAL(static_cast<uint8_t>(OrreryMessageType::LeaderCommand), static_cast<uint8_t>(type));
  TEST_ASSERT_EQUAL_UINT32(msg1.leaderBootId, msg2.leaderBootId);
  TEST_ASSERT_EQUAL_UINT32(msg1.leaderSequenceNumber, msg2.leaderSequenceNumber);
  TEST_ASSERT(msg2.speed.has_value());
  TEST_ASSERT_EQUAL_INT32(*msg1.speed, *msg2.speed);
  TEST_ASSERT(msg2.position.has_value());
  TEST_ASSERT_EQUAL_UINT32(*msg1.position, *msg2.position);
  TEST_ASSERT(msg2.calibration.has_value());
  TEST_ASSERT_EQUAL_UINT32(*msg1.calibration, *msg2.calibration);
  TEST_ASSERT(msg2.ledPattern.has_value());
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(*msg1.ledPattern), static_cast<uint32_t>(*msg2.ledPattern));
  TEST_ASSERT(msg2.ledBrightness.has_value());
  TEST_ASSERT_EQUAL_UINT8(*msg1.ledBrightness, *msg2.ledBrightness);
  TEST_ASSERT(msg2.ledPrecedence.has_value());
  TEST_ASSERT_EQUAL_UINT16(*msg1.ledPrecedence, *msg2.ledPrecedence);
  TEST_ASSERT(reader.Done());
}

void test_orrery_message_sparse_serialization() {
  OrreryMessage msg1;
  msg1.leaderBootId = 0x11223344;
  msg1.leaderSequenceNumber = 0x55667788;
  msg1.speed = std::nullopt;
  msg1.position = 12345;
  msg1.calibration = std::nullopt;
  msg1.ledPattern = std::nullopt;
  msg1.ledBrightness = 128;
  msg1.ledPrecedence = std::nullopt;

  uint8_t buffer[64];
  NetworkWriter writer(buffer, sizeof(buffer));
  TEST_ASSERT(WriteOrreryMessage(OrreryMessageType::FollowerResponse, msg1, writer));

  NetworkReader reader(buffer, writer.LengthWritten());
  OrreryMessageType type;
  OrreryMessage msg2;
  TEST_ASSERT(ReadOrreryMessage(reader, &type, &msg2));

  TEST_ASSERT_EQUAL(static_cast<uint8_t>(OrreryMessageType::FollowerResponse), static_cast<uint8_t>(type));
  TEST_ASSERT_EQUAL_UINT32(msg1.leaderBootId, msg2.leaderBootId);
  TEST_ASSERT_EQUAL_UINT32(msg1.leaderSequenceNumber, msg2.leaderSequenceNumber);
  TEST_ASSERT_FALSE(msg2.speed.has_value());
  TEST_ASSERT(msg2.position.has_value());
  TEST_ASSERT_EQUAL_UINT32(*msg1.position, *msg2.position);
  TEST_ASSERT_FALSE(msg2.calibration.has_value());
  TEST_ASSERT_FALSE(msg2.ledPattern.has_value());
  TEST_ASSERT(msg2.ledBrightness.has_value());
  TEST_ASSERT_EQUAL_UINT8(*msg1.ledBrightness, *msg2.ledBrightness);
  TEST_ASSERT_FALSE(msg2.ledPrecedence.has_value());
  TEST_ASSERT(reader.Done());
}

void run_unity_tests() {
  UNITY_BEGIN();
  RUN_TEST(test_orrery_message_serialization);
  RUN_TEST(test_orrery_message_sparse_serialization);
  UNITY_END();
}

}  // namespace jazzlights

void setUp() {}

void tearDown() {}

#ifdef ESP32

void setup() { jazzlights::run_unity_tests(); }

void loop() {}

#else  // ESP32

int main(int /*argc*/, char** /*argv*/) {
  jazzlights::run_unity_tests();
  return 0;
}

#endif  // ESP32
