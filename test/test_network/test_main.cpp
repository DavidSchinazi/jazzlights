#include <unity.h>

#include "jazzlights/network/network.h"

namespace jazzlights {

void test_network_reader() {
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
void test_network_writer() {
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

void test_network_int32() {
  int32_t values[] = {0, -1, 2147483647, -2147483648};
  uint8_t expected_bytes[][4] = {
      {0x00, 0x00, 0x00, 0x00},
      {0xFF, 0xFF, 0xFF, 0xFF},
      {0x7F, 0xFF, 0xFF, 0xFF},
      {0x80, 0x00, 0x00, 0x00},
  };

  for (size_t i = 0; i < 4; ++i) {
    uint8_t buffer[4];
    NetworkWriter writer(buffer, sizeof(buffer));
    TEST_ASSERT(writer.WriteInt32(values[i]));
    for (size_t j = 0; j < 4; ++j) { TEST_ASSERT_EQUAL_HEX8(expected_bytes[i][j], buffer[j]); }

    NetworkReader reader(buffer, sizeof(buffer));
    int32_t out;
    TEST_ASSERT(reader.ReadInt32(&out));
    TEST_ASSERT_EQUAL_INT32(values[i], out);
  }
}

void run_unity_tests() {
  UNITY_BEGIN();
  RUN_TEST(test_network_reader);
  RUN_TEST(test_network_writer);
  RUN_TEST(test_network_int32);
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
