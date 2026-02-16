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

#define jll_cobs_data_buffer2(bufferClass, ...) jll_cobs_data_buffer(&bufferClass[0], bufferClass.size(), ##__VA_ARGS__)

namespace jazzlights {

void CobsEncode(const BufferViewU8 inputBuffer, BufferViewU8* encodedOutputBuffer) {
  jll_cobs_data_buffer2(inputBuffer, "COBS encode");
  uint8_t code = 0x01;
  size_t inputIndex = 0;
  size_t outputCodeIndex = 0;
  size_t outputIndex = 1;
  while (inputIndex < inputBuffer.size()) {
    if (outputIndex >= encodedOutputBuffer->size()) {
      jll_error("CobsEncode ran out of space");
      encodedOutputBuffer->resize(0);
      return;
    }
    if (inputBuffer[inputIndex] == 0x00) {
      (*encodedOutputBuffer)[outputCodeIndex] = code;
      code = 0x01;
      outputCodeIndex = outputIndex;
      outputIndex++;
    } else {
      (*encodedOutputBuffer)[outputIndex] = inputBuffer[inputIndex];
      outputIndex++;
      code++;
      if (code == 0xFF) {
        (*encodedOutputBuffer)[outputCodeIndex] = code;
        code = 0x01;
        outputCodeIndex = outputIndex;
        outputIndex++;
      }
    }
    inputIndex++;
  }
  (*encodedOutputBuffer)[outputCodeIndex] = code;
  encodedOutputBuffer->resize(outputIndex);
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
