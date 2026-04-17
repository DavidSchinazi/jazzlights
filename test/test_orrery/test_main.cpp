#include <unity.h>

#include "jazzlights/network/network.h"
#include "jazzlights/orrery_common.h"

namespace jazzlights {

void test_orrery_message_serialization() {
  OrreryMessage msg1;
  msg1.type = OrreryMessageType::LeaderCommand;
  msg1.leaderBootId = 0x12345678;
  msg1.leaderSequenceNumber = 0x87654321;
  msg1.speed = 1000;
  msg1.position = 5000;
  msg1.calibration = 0;
  msg1.ledPattern = 0xABCDEF;
  msg1.ledBrightness = 200;
  msg1.ledBasePrecedence = 10000;
  msg1.ledPrecedenceGain = 500;

  uint8_t buffer[64];
  NetworkWriter writer(buffer, sizeof(buffer));
  TEST_ASSERT(WriteOrreryMessage(msg1, writer));

  NetworkReader reader(buffer, writer.LengthWritten());
  OrreryMessage msg2;
  TEST_ASSERT(ReadOrreryMessage(reader, &msg2));

  TEST_ASSERT_EQUAL(static_cast<uint8_t>(OrreryMessageType::LeaderCommand), static_cast<uint8_t>(msg2.type));
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
  TEST_ASSERT(msg2.ledBasePrecedence.has_value());
  TEST_ASSERT_EQUAL_UINT16(*msg1.ledBasePrecedence, *msg2.ledBasePrecedence);
  TEST_ASSERT(msg2.ledPrecedenceGain.has_value());
  TEST_ASSERT_EQUAL_UINT16(*msg1.ledPrecedenceGain, *msg2.ledPrecedenceGain);
  TEST_ASSERT(reader.Done());
}

void test_orrery_message_hall_sensor_serialization() {
  OrreryMessage msg1;
  msg1.type = OrreryMessageType::FollowerResponse;
  msg1.leaderBootId = 0xDEADBEEF;
  msg1.leaderSequenceNumber = 0xCAFEBABE;
  msg1.timeHallSensorLastOpened = 10000;
  msg1.timeHallSensorLastClosed = 20000;
  msg1.lastOpenDuration = 1234;
  msg1.lastClosedDuration = 5678;

  uint8_t buffer[64];
  NetworkWriter writer(buffer, sizeof(buffer));
  TEST_ASSERT(WriteOrreryMessage(msg1, writer));

  NetworkReader reader(buffer, writer.LengthWritten());
  OrreryMessage msg2;
  TEST_ASSERT(ReadOrreryMessage(reader, &msg2));

  TEST_ASSERT_EQUAL(static_cast<uint8_t>(OrreryMessageType::FollowerResponse), static_cast<uint8_t>(msg2.type));
  TEST_ASSERT(msg2.timeHallSensorLastOpened.has_value());
  // Note: timeHallSensorLastOpened is relative to timeMillis() during Write/Read
  // So we can only test approximate equality if we want to be safe, but since this is native and fast, it should be
  // exact if timeMillis() doesn't change. Actually, WriteOrreryMessage uses timeMillis() -
  // *msg.timeHallSensorLastOpened. ReadOrreryMessage uses timeMillis() - delta. So msg2.timeHallSensorLastOpened =
  // timeMillis() - (timeMillis() - msg1.timeHallSensorLastOpened) = msg1.timeHallSensorLastOpened.
  TEST_ASSERT_EQUAL_UINT32(*msg1.timeHallSensorLastOpened, *msg2.timeHallSensorLastOpened);
  TEST_ASSERT(msg2.timeHallSensorLastClosed.has_value());
  TEST_ASSERT_EQUAL_UINT32(*msg1.timeHallSensorLastClosed, *msg2.timeHallSensorLastClosed);
  TEST_ASSERT(msg2.lastOpenDuration.has_value());
  TEST_ASSERT_EQUAL_UINT32(*msg1.lastOpenDuration, *msg2.lastOpenDuration);
  TEST_ASSERT(msg2.lastClosedDuration.has_value());
  TEST_ASSERT_EQUAL_UINT32(*msg1.lastClosedDuration, *msg2.lastClosedDuration);
  TEST_ASSERT(reader.Done());
}

void test_orrery_message_sparse_serialization() {
  OrreryMessage msg1;
  msg1.type = OrreryMessageType::FollowerResponse;
  msg1.leaderBootId = 0x11223344;
  msg1.leaderSequenceNumber = 0x55667788;
  msg1.speed = std::nullopt;
  msg1.position = 12345;
  msg1.calibration = std::nullopt;
  msg1.ledPattern = std::nullopt;
  msg1.ledBrightness = 128;
  msg1.ledBasePrecedence = std::nullopt;
  msg1.ledPrecedenceGain = std::nullopt;

  uint8_t buffer[64];
  NetworkWriter writer(buffer, sizeof(buffer));
  TEST_ASSERT(WriteOrreryMessage(msg1, writer));

  NetworkReader reader(buffer, writer.LengthWritten());
  OrreryMessage msg2;
  TEST_ASSERT(ReadOrreryMessage(reader, &msg2));

  TEST_ASSERT_EQUAL(static_cast<uint8_t>(OrreryMessageType::FollowerResponse), static_cast<uint8_t>(msg2.type));
  TEST_ASSERT_EQUAL_UINT32(msg1.leaderBootId, msg2.leaderBootId);
  TEST_ASSERT_EQUAL_UINT32(msg1.leaderSequenceNumber, msg2.leaderSequenceNumber);
  TEST_ASSERT_FALSE(msg2.speed.has_value());
  TEST_ASSERT(msg2.position.has_value());
  TEST_ASSERT_EQUAL_UINT32(*msg1.position, *msg2.position);
  TEST_ASSERT_FALSE(msg2.calibration.has_value());
  TEST_ASSERT_FALSE(msg2.ledPattern.has_value());
  TEST_ASSERT(msg2.ledBrightness.has_value());
  TEST_ASSERT_EQUAL_UINT8(*msg1.ledBrightness, *msg2.ledBrightness);
  TEST_ASSERT_FALSE(msg2.ledBasePrecedence.has_value());
  TEST_ASSERT_FALSE(msg2.ledPrecedenceGain.has_value());
  TEST_ASSERT(reader.Done());
}

void run_unity_tests() {
  UNITY_BEGIN();
  RUN_TEST(test_orrery_message_serialization);
  RUN_TEST(test_orrery_message_hall_sensor_serialization);
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
