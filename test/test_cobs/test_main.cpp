#include <unity.h>

#include <cstring>
#include <iostream>

#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/cobs.h"

namespace jazzlights {

void test_cobs_buffer(uint8_t* inputBuffer, size_t inputLength) {
  if (inputBuffer == nullptr) {
    TEST_FAIL_MESSAGE("empty input");
    return;
  }
  size_t encodeBufferLength = CobsMaxEncodedSize(inputLength);
  size_t decodeBufferLength = inputLength;
  uint8_t* encodeBuffer = reinterpret_cast<uint8_t*>(malloc(encodeBufferLength));
  uint8_t* decodeBuffer = reinterpret_cast<uint8_t*>(malloc(decodeBufferLength));
  do {
    if (encodeBuffer == nullptr || decodeBuffer == nullptr) {
      TEST_FAIL_MESSAGE("malloc failure");
      break;
    }
    size_t encodedLength = CobsEncode(inputBuffer, inputLength, encodeBuffer, encodeBufferLength);
    if (encodedLength <= 0) {
      TEST_FAIL_MESSAGE("encode failure");
      break;
    }
    bool bad_encoding = false;
    for (size_t i = 0; i < encodedLength; ++i) {
      if (encodeBuffer[i] == 0x00) {
        bad_encoding = true;
        break;
      }
    }
    if (bad_encoding) {
      TEST_FAIL_MESSAGE("zero found in encoding");
      break;
    }
    size_t decodedLength = CobsDecode(encodeBuffer, encodedLength, decodeBuffer, decodeBufferLength);
    if (decodedLength <= 0) {
      TEST_FAIL_MESSAGE("decode failure");
      break;
    }
    if (decodedLength != inputLength) {
      TEST_FAIL_MESSAGE("decode length mismatch");
      break;
    }
    int res = memcmp(inputBuffer, decodeBuffer, inputLength);
    if (res != 0) {
      TEST_FAIL_MESSAGE("decode mismatch");
      break;
    }
  } while (false);
  free(encodeBuffer);
  free(decodeBuffer);
}

void test_cobs(size_t inputLength) {
  uint8_t* inputBuffer = reinterpret_cast<uint8_t*>(malloc(inputLength));
  do {
    if (inputBuffer == nullptr) {
      TEST_FAIL_MESSAGE("malloc failure");
      break;
    }
    for (size_t i = 0; i < inputLength; ++i) { inputBuffer[i] = UnpredictableRandom::GetByte(); }
    test_cobs_buffer(inputBuffer, inputLength);
  } while (false);
  free(inputBuffer);
}

void test_short() { test_cobs(10); }
void test_big() { test_cobs(1000); }
void test_multiple() {
  for (size_t i = 1; i < 10000; i += 10) { test_cobs(i); }
}

void run_unity_tests() {
  UNITY_BEGIN();
  RUN_TEST(test_short);
  RUN_TEST(test_big);
  RUN_TEST(test_multiple);
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
