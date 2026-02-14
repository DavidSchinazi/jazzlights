#include "jazzlights/util/cobs.h"

#include <cstddef>
#include <cstdint>

namespace jazzlights {

size_t CobsEncode(const uint8_t* inputBuffer, size_t inputLength, uint8_t* encodedOuputBuffer,
                  size_t encodedOuputBufferSize) {
  uint8_t code = 0x01;
  size_t inputIndex = 0;
  size_t outputCodeIndex = 0;
  size_t outputIndex = 1;
  while (inputIndex < inputLength) {
    if (outputIndex >= encodedOuputBufferSize) { return 0; }
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
      if (outputIndex >= outputBufferSize) { return 0; }
      outputBuffer[outputIndex] = encodedInputBuffer[inputIndex];
      outputIndex++;
      inputIndex++;
    }
    if (code < 0xFF) {
      if (inputIndex == encodedInputLength) { return outputIndex; }
      if (outputIndex >= outputBufferSize) { return 0; }
      outputBuffer[outputIndex] = 0x00;
      outputIndex++;
    }
  }
  return outputIndex;
}

size_t CobsMaxEncodedSize(size_t inputLength) {
  return static_cast<size_t>((((static_cast<uint64_t>(inputLength) * 255) + 253) / 254));
}

}  // namespace jazzlights
