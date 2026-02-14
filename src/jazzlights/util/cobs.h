
#ifndef JL_UTIL_COBS_H
#define JL_UTIL_COBS_H

#include <cstddef>
#include <cstdint>

namespace jazzlights {

// COBS (Consistent Overhead Byte Stuffing) is a byte encoding free of zero-bytes that minimizes the maximal encoding
// overhead from the original input. See <https://www.stuartcheshire.org/papers/COBSforToN.pdf> for details.

size_t CobsEncode(const uint8_t* inputBuffer, size_t inputLength, uint8_t* encodedOuputBuffer,
                  size_t encodedOuputBufferSize);

size_t CobsDecode(const uint8_t* encodedInputBuffer, size_t encodedInputLength, uint8_t* outputBuffer,
                  size_t outputBufferSize);

constexpr size_t CobsMaxEncodedSize(size_t inputLength) {
  return static_cast<size_t>((((static_cast<uint64_t>(inputLength) * 255) + 253) / 254));
}

}  // namespace jazzlights

#endif  // JL_UTIL_COBS_H
