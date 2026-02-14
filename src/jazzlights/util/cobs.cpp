#include "jazzlights/util/cobs.h"

#include <cstddef>
#include <cstdint>

#include "jazzlights/util/log.h"

#ifndef JL_LOG_COBS_DATA
#define JL_LOG_COBS_DATA 0
#endif  // JL_LOG_COBS_DATA

#if JL_LOG_COBS_DATA
#define jll_cobs_data(...) jll_info(__VA_ARGS__)
#define jll_cobs_data_buffer(...) jll_buffer_info(__VA_ARGS__)
#else  // JL_LOG_COBS_DATA
#define jll_cobs_data(...) jll_debug(__VA_ARGS__)
#define jll_cobs_data_buffer(...) jll_buffer_debug(__VA_ARGS__)
#endif  // JL_LOG_COBS_DATA

namespace jazzlights {

size_t CobsEncode(const uint8_t* inputBuffer, size_t inputLength, uint8_t* encodedOuputBuffer,
                  size_t encodedOuputBufferSize) {
  jll_cobs_data_buffer(inputBuffer, inputLength, "COBS encode");
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
        jll_cobs_data_buffer(outputBuffer, outputIndex, "COBS decode1");
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
  jll_cobs_data_buffer(outputBuffer, outputIndex, "COBS decode2");
  return outputIndex;
}

}  // namespace jazzlights
