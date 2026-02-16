
#ifndef JL_UTIL_COBS_H
#define JL_UTIL_COBS_H

#include <cstddef>
#include <cstdint>

#include "jazzlights/util/buffer.h"

namespace jazzlights {

// COBS (Consistent Overhead Byte Stuffing) is a byte encoding free of zero-bytes that minimizes the maximal encoding
// overhead from the original input. See <https://www.stuartcheshire.org/papers/COBSforToN.pdf> for details.

void CobsEncode(const BufferViewU8 inputBuffers[], size_t numInputBuffers, BufferViewU8* encodedOutputBuffer);

inline void CobsEncode(const BufferViewU8 inputBuffer, BufferViewU8* encodedOutputBuffer) {
  CobsEncode(&inputBuffer, 1, encodedOutputBuffer);
}

void CobsDecode(const BufferViewU8 encodedInputBuffer, BufferViewU8* outputBuffer);

constexpr size_t CobsMaxEncodedSize(size_t inputLength) {
  return static_cast<size_t>((((static_cast<uint64_t>(inputLength) * 255) + 253) / 254));
}

}  // namespace jazzlights

#endif  // JL_UTIL_COBS_H
