#include <unity.h>

#include <cstring>
#include <iostream>

#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/buffer.h"
#include "jazzlights/util/cobs.h"

namespace jazzlights {

void test_cobs_buffers(const BufferViewU8 inputBuffers[], size_t numInputBuffers) {
  size_t inputBufferLength = 0;
  for (size_t i = 0; i < numInputBuffers; ++i) { inputBufferLength += inputBuffers[i].size(); }
  size_t encodeBufferLength = CobsMaxEncodedSize(inputBufferLength);
  size_t decodeBufferLength = inputBufferLength;
  OwnedBufferU8 encodeBuffer(encodeBufferLength);
  OwnedBufferU8 decodeBuffer(decodeBufferLength);
  OwnedBufferU8 testBuffer(inputBufferLength);
  size_t testBufferOffset = 0;
  for (size_t i = 0; i < numInputBuffers; ++i) {
    memcpy(&testBuffer[testBufferOffset], &inputBuffers[i][0], inputBuffers[i].size());
    testBufferOffset += inputBuffers[i].size();
  }
  do {
    BufferViewU8 encodedBuffer(encodeBuffer);
    CobsEncode(inputBuffers, numInputBuffers, &encodedBuffer);
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
    if (decodedBuffer.size() != inputBufferLength) {
      TEST_FAIL_MESSAGE("decode length mismatch");
      break;
    }
    int res = memcmp(&testBuffer[0], &decodedBuffer[0], inputBufferLength);
    if (res != 0) {
      TEST_FAIL_MESSAGE("decode mismatch");
      break;
    }
  } while (false);
}

void test_cobs_buffer(const BufferViewU8 inputBuffer) { test_cobs_buffers(&inputBuffer, 1); }

void test_cobs(size_t inputLength) {
  OwnedBufferU8 inputBuffer(inputLength);
  for (size_t i = 0; i < inputBuffer.size(); ++i) { inputBuffer[i] = UnpredictableRandom::GetByte(); }
  test_cobs_buffer(inputBuffer);
}

void test_cobs_split(size_t inputLength) {
  size_t inputLength1 = UnpredictableRandom::GetNumberBetween(0, inputLength);
  size_t inputLength2 = inputLength - inputLength1;
  OwnedBufferU8 inputBuffer1(inputLength1);
  OwnedBufferU8 inputBuffer2(inputLength2);
  for (size_t i = 0; i < inputBuffer1.size(); ++i) { inputBuffer1[i] = UnpredictableRandom::GetByte(); }
  for (size_t i = 0; i < inputBuffer2.size(); ++i) { inputBuffer2[i] = UnpredictableRandom::GetByte(); }
  BufferViewU8 inputs[] = {inputBuffer1, inputBuffer2};
  test_cobs_buffers(inputs, 2);
}

void test_short() { test_cobs(10); }
void test_big() { test_cobs(1000); }
void test_multiple() {
  for (size_t i = 1; i < 10000; i += 10) { test_cobs(i); }
}
void test_multiple_split() {
  for (size_t i = 1; i < 10000; i += 10) { test_cobs_split(i); }
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
