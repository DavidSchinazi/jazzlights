#include "jazzlights/util/cobs.h"

#include <cstddef>
#include <cstdint>

#include "jazzlights/util/log.h"

#define JL_COBS_PRINT 0

#if JL_COBS_PRINT
static constexpr size_t kCobsPrintBufferSize = 2048;
#endif  // JL_COBS_PRINT

namespace jazzlights {

size_t CobsEncode(const uint8_t* inputBuffer, size_t inputLength, uint8_t* encodedOuputBuffer,
                  size_t encodedOuputBufferSize) {
#if JL_COBS_PRINT
  static uint8_t* cobsPrintBuffer = reinterpret_cast<uint8_t*>(malloc(kCobsPrintBufferSize));
  EscapeRawBuffer(inputBuffer, inputLength, cobsPrintBuffer, kCobsPrintBufferSize);
  jll_info("COBS encode: %s", cobsPrintBuffer);
#endif  // JL_COBS_PRINT
  uint8_t code = 0x01;
  size_t inputIndex = 0;
  size_t outputCodeIndex = 0;
  size_t outputIndex = 1;
  while (inputIndex < inputLength) {
    if (outputIndex >= encodedOuputBufferSize) {
      jll_error("CobsEncode ran out of space");
      return 0;
    }
    if (inputBuffer[inputIndex] == 0x00) {
      encodedOuputBuffer[outputCodeIndex] = code;
      code = 0x01;
      outputCodeIndex = outputIndex;
      outputIndex++;
    } else {
      encodedOuputBuffer[outputIndex] = inputBuffer[inputIndex];
      outputIndex++;
      code++;
      if (code == 0xFF) {
        encodedOuputBuffer[outputCodeIndex] = code;
        code = 0x01;
        outputCodeIndex = outputIndex;
        outputIndex++;
      }
    }
    inputIndex++;
  }
  encodedOuputBuffer[outputCodeIndex] = code;
  return outputIndex;
}

size_t CobsDecode(const uint8_t* encodedInputBuffer, size_t encodedInputLength, uint8_t* outputBuffer,
                  size_t outputBufferSize) {
  size_t inputIndex = 0;
  size_t outputIndex = 0;
  while (inputIndex < encodedInputLength) {
    const uint8_t code = encodedInputBuffer[inputIndex];
    inputIndex++;
    for (uint8_t i = 1; i < code; ++i) {
      if (outputIndex >= outputBufferSize) {
        jll_error("CobsDecode ran out of space 1");
        return 0;
      }
      outputBuffer[outputIndex] = encodedInputBuffer[inputIndex];
      outputIndex++;
      inputIndex++;
    }
    if (code < 0xFF) {
      if (inputIndex == encodedInputLength) {
#if JL_COBS_PRINT
        static uint8_t* cobsPrintBuffer = reinterpret_cast<uint8_t*>(malloc(kCobsPrintBufferSize));
        EscapeRawBuffer(outputBuffer, outputIndex, cobsPrintBuffer, kCobsPrintBufferSize);
        jll_info("COBS decode1: %s", cobsPrintBuffer);
#endif  // JL_COBS_PRINT
        return outputIndex;
      }
      if (outputIndex >= outputBufferSize) {
        jll_error("CobsDecode ran out of space 2");
        return 0;
      }
      outputBuffer[outputIndex] = 0x00;
      outputIndex++;
    }
  }
#if JL_COBS_PRINT
  static uint8_t* cobsPrintBuffer = reinterpret_cast<uint8_t*>(malloc(kCobsPrintBufferSize));
  EscapeRawBuffer(outputBuffer, outputIndex, cobsPrintBuffer, kCobsPrintBufferSize);
  jll_info("COBS decode2: %s", cobsPrintBuffer);
#endif  // JL_COBS_PRINT
  return outputIndex;
}

}  // namespace jazzlights
