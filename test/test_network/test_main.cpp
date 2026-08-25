#include <unity.h>

#include "jazzlights/network/network.h"
#include "jazzlights/protocol/reader.h"
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

void RunUnityTests() {
  UNITY_BEGIN();
  RUN_TEST(TestNetworkReader);
  RUN_TEST(TestNetworkWriter);
  RUN_TEST(TestNetworkInt32);
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
