#include <unity.h>

#include <cstring>
#include <iostream>

#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/buffer.h"
#include "jazzlights/util/cobs.h"

namespace jazzlights {

void test_cobs_buffer(const BufferViewU8 inputBuffer) {
  size_t encodeBufferLength = CobsMaxEncodedSize(inputBuffer.size());
  size_t decodeBufferLength = inputBuffer.size();
  OwnedBufferU8 encodeBuffer(encodeBufferLength);
  OwnedBufferU8 decodeBuffer(decodeBufferLength);
  do {
    BufferViewU8 encodedBuffer(encodeBuffer);
    CobsEncode(inputBuffer, &encodedBuffer);
    if (encodedBuffer.size() <= 0) {
      TEST_FAIL_MESSAGE("encode failure");
      break;
    }
    bool bad_encoding = false;
    for (size_t i = 0; i < encodedBuffer.size(); ++i) {
      if (encodeBuffer[i] == 0x00) {
        bad_encoding = true;
        break;
      }
    }
    if (bad_encoding) {
      TEST_FAIL_MESSAGE("zero found in encoding");
      break;
    }
    BufferViewU8 decodedBuffer(decodeBuffer);
    CobsDecode(encodedBuffer, &decodedBuffer);
    if (decodedBuffer.size() <= 0) {
      TEST_FAIL_MESSAGE("decode failure");
      break;
    }
    if (decodedBuffer.size() != inputBuffer.size()) {
      TEST_FAIL_MESSAGE("decode length mismatch");
      break;
    }
    int res = memcmp(&inputBuffer[0], &decodedBuffer[0], inputBuffer.size());
    if (res != 0) {
      TEST_FAIL_MESSAGE("decode mismatch");
      break;
    }
  } while (false);
}

void test_cobs(size_t inputLength) {
  OwnedBufferU8 inputBuffer(inputLength);
  for (size_t i = 0; i < inputBuffer.size(); ++i) { inputBuffer[i] = UnpredictableRandom::GetByte(); }
  test_cobs_buffer(inputBuffer);
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
