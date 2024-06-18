#include <unity.h>

#include "jazzlights/network.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif  // ARDUINO

namespace jazzlights {

void test_network_reader(void) {
  uint8_t buffer[7] = {1, 2, 3, 4, 5, 6, 7};
  NetworkReader reader(buffer, sizeof(buffer));
  uint8_t u8;
  TEST_ASSERT(reader.ReadUint8(&u8));
  TEST_ASSERT_EQUAL(u8, 0x01);
  uint16_t u16;
  TEST_ASSERT(reader.ReadUint16(&u16));
  TEST_ASSERT_EQUAL(u16, 0x0203);
  uint32_t u32;
  TEST_ASSERT(reader.ReadUint32(&u32));
  TEST_ASSERT_EQUAL(u32, 0x04050607);
  TEST_ASSERT_FALSE(reader.ReadUint8(&u8));
}
void test_network_writer(void) {
  uint8_t buffer[7] = {};
  NetworkWriter writer(buffer, sizeof(buffer));
  uint8_t u8 = 1;
  TEST_ASSERT(writer.WriteUint8(u8));
  TEST_ASSERT_EQUAL(buffer[0], 0x01);
  uint16_t u16 = 0x0203;
  TEST_ASSERT(writer.WriteUint16(u16));
  TEST_ASSERT_EQUAL(buffer[1], 0x02);
  TEST_ASSERT_EQUAL(buffer[2], 0x03);
  uint32_t u32 = 0x04050607;
  TEST_ASSERT(writer.WriteUint32(u32));
  TEST_ASSERT_EQUAL(buffer[3], 0x04);
  TEST_ASSERT_EQUAL(buffer[4], 0x05);
  TEST_ASSERT_EQUAL(buffer[5], 0x06);
  TEST_ASSERT_EQUAL(buffer[6], 0x07);
  TEST_ASSERT_FALSE(writer.WriteUint8(u8));
}

void run_unity_tests() {
  UNITY_BEGIN();
  RUN_TEST(test_network_reader);
  RUN_TEST(test_network_writer);
  UNITY_END();
}

}  // namespace jazzlights

void setUp(void) {}

void tearDown(void) {}

#ifdef ARDUINO

void setup() {
  // The 2s delay is required if board doesn't support software reset via Serial.DTR/RTS.
  delay(2000);

  jazzlights::run_unity_tests();
}

void loop() {}

#else  // ARDUINO

int main(int /*argc*/, char** /*argv*/) {
  jazzlights::run_unity_tests();
  return 0;
}

#endif  // ARDUINO
