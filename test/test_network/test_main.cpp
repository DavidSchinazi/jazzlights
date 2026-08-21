#include <unity.h>

#include "jazzlights/network/ble_payload.h"
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

#if !JL_IS_CONFIG(CREATURE)
// The non-CREATURE encode/decode path is fixed-offset raw reads/writes (see ble_payload.cpp); the
// CREATURE path uses NetworkReader/NetworkWriter but isn't covered here since [env:native]'s
// default config is non-CREATURE.
void test_ble_payload_round_trip() {
  const uint8_t originatorBytes[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
  NetworkMessage original;
  original.originator = NetworkDeviceId(originatorBytes);
  original.precedence = 1234;
  original.numHops = 3;
  original.currentPattern = 0x11223344;
  original.nextPattern = 0x55667788;
  original.currentPatternStartTime = 1000;
  original.lastOriginationTime = 500;

  const Milliseconds sendTime = 2000;
  uint8_t buffer[kBleMaxInnerPayloadLength];
  uint8_t len = EncodeBleInnerPayload(original, sendTime, buffer, sizeof(buffer));
  TEST_ASSERT(len > 0);

  const uint8_t senderBytes[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
  const NetworkDeviceId sender(senderBytes);
  const Milliseconds receiveTime = 2100;
  NetworkMessage decoded;
  Milliseconds receiptTime;
  TEST_ASSERT(DecodeBleInnerPayload(sender, buffer, len, /*rssi=*/-40, receiveTime, &decoded, &receiptTime));

  TEST_ASSERT(decoded.sender == sender);
  TEST_ASSERT(decoded.originator == original.originator);
  TEST_ASSERT_EQUAL(decoded.precedence, original.precedence);
  TEST_ASSERT_EQUAL(decoded.numHops, original.numHops);
  TEST_ASSERT_EQUAL(decoded.currentPattern, original.currentPattern);
  TEST_ASSERT_EQUAL(decoded.nextPattern, original.nextPattern);

  // currentPatternStartTime/lastOriginationTime are intentionally lossy (compressed into 16-bit
  // deltas and reconstructed via a fixed kTransmissionOffset compensation) -- reproduce that math
  // instead of expecting exact equality against the originally-sent values.
  constexpr Milliseconds kTransmissionOffset = 25;
  const Milliseconds expectedReceiptTime = receiveTime - kTransmissionOffset;
  const uint16_t patternTimeDelta = static_cast<uint16_t>(sendTime - original.currentPatternStartTime);
  const uint16_t originationTimeDelta = static_cast<uint16_t>(sendTime - original.lastOriginationTime);
  TEST_ASSERT_EQUAL(receiptTime, expectedReceiptTime);
  TEST_ASSERT_EQUAL(decoded.currentPatternStartTime, expectedReceiptTime - patternTimeDelta);
  TEST_ASSERT_EQUAL(decoded.lastOriginationTime, expectedReceiptTime - originationTimeDelta);
}

void test_ble_payload_decode_too_short() {
  uint8_t buffer[5] = {0, 1, 2, 3, 4};
  NetworkMessage decoded;
  Milliseconds receiptTime;
  TEST_ASSERT_FALSE(
      DecodeBleInnerPayload(NetworkDeviceId(), buffer, sizeof(buffer), -40, 1000, &decoded, &receiptTime));
}

void test_ble_payload_decode_too_long() {
  uint8_t buffer[kBleMaxInnerPayloadLength + 1] = {};
  NetworkMessage decoded;
  Milliseconds receiptTime;
  TEST_ASSERT_FALSE(
      DecodeBleInnerPayload(NetworkDeviceId(), buffer, sizeof(buffer), -40, 1000, &decoded, &receiptTime));
}
#endif  // !JL_IS_CONFIG(CREATURE)

void test_find_ad_structure_with_leading_flags() {
  // Realistic NimBLE case: a leading Flags (0x01) AD structure ahead of ours.
  // [len=2][type=0x01 flags][data=0x06] then [len=4][type=kBleAdvType][payload=0xAA,0xBB,0xCC]
  uint8_t blob[] = {0x02, 0x01, 0x06, 0x04, kBleAdvType, 0xAA, 0xBB, 0xCC};
  const uint8_t* inner = nullptr;
  uint8_t innerLen = 0;
  TEST_ASSERT(FindJazzLightsAdStructure(blob, sizeof(blob), &inner, &innerLen));
  TEST_ASSERT_EQUAL(innerLen, 3);
  TEST_ASSERT_EQUAL_HEX8(inner[0], 0xAA);
  TEST_ASSERT_EQUAL_HEX8(inner[1], 0xBB);
  TEST_ASSERT_EQUAL_HEX8(inner[2], 0xCC);
}

void test_find_ad_structure_missing() {
  uint8_t blob[] = {0x02, 0x01, 0x06, 0x02, 0xFF, 0x00};
  const uint8_t* inner = nullptr;
  uint8_t innerLen = 0;
  TEST_ASSERT_FALSE(FindJazzLightsAdStructure(blob, sizeof(blob), &inner, &innerLen));
}

void test_find_ad_structure_truncated() {
  // Claims a 5-byte structure but only 3 bytes are actually present.
  uint8_t blob[] = {0x05, kBleAdvType, 0xAA};
  const uint8_t* inner = nullptr;
  uint8_t innerLen = 0;
  TEST_ASSERT_FALSE(FindJazzLightsAdStructure(blob, sizeof(blob), &inner, &innerLen));
}

void run_unity_tests() {
  UNITY_BEGIN();
  RUN_TEST(test_network_reader);
  RUN_TEST(test_network_writer);
  RUN_TEST(test_network_int32);
#if !JL_IS_CONFIG(CREATURE)
  RUN_TEST(test_ble_payload_round_trip);
  RUN_TEST(test_ble_payload_decode_too_short);
  RUN_TEST(test_ble_payload_decode_too_long);
#endif  // !JL_IS_CONFIG(CREATURE)
  RUN_TEST(test_find_ad_structure_with_leading_flags);
  RUN_TEST(test_find_ad_structure_missing);
  RUN_TEST(test_find_ad_structure_truncated);
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
