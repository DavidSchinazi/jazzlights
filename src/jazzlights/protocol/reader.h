#ifndef JL_PROTOCOL_READER_H
#define JL_PROTOCOL_READER_H

#include <cstddef>
#include <cstdint>
#include <optional>

#include "jazzlights/network/network.h"
#include "jazzlights/util/time.h"
#include "jazzlights/util/types.h"

namespace jazzlights {

class ProtocolReader {
 public:
  explicit ProtocolReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

  bool Done() const { return pos_ >= size_; }

  bool ReadUint8(uint8_t* out) {
    if (sizeof(*out) > size_ || pos_ > size_ - sizeof(*out)) { return false; }
    *out = data_[pos_];
    pos_ += sizeof(*out);
    return true;
  }

  bool ReadUint16(uint16_t* out) {
    if (sizeof(*out) > size_ || pos_ > size_ - sizeof(*out)) { return false; }
    *out = (data_[pos_] << 8) | (data_[pos_ + 1]);
    pos_ += sizeof(*out);
    return true;
  }

  bool ReadUint32(uint32_t* out) {
    if (sizeof(*out) > size_ || pos_ > size_ - sizeof(*out)) { return false; }
    *out = (data_[pos_] << 24) | (data_[pos_ + 1] << 16) | (data_[pos_ + 2] << 8) | (data_[pos_ + 3]);
    pos_ += sizeof(*out);
    return true;
  }

  // Uses two's complement.
  bool ReadInt32(int32_t* out) {
    uint32_t u;
    if (!ReadUint32(&u)) { return false; }
    if (u <= 0x7FFFFFFFU) {
      *out = static_cast<int32_t>(u);
    } else {
      *out = -static_cast<int32_t>(0xFFFFFFFFU - u) - 1;
    }
    return true;
  }

  bool ReadPatternBits(PatternBits* out) {
    // In theory we shouldn't need this because PatternBits is roughly a uint32_t, but we made it an unsigned int to
    // allow `printf("%u", pattern)` without warnings.
    return ReadUint32(reinterpret_cast<uint32_t*>(out));
  }

  bool ReadNetworkDeviceId(NetworkDeviceId* out) {
    if (NetworkDeviceId::size() > size_ || pos_ > size_ - NetworkDeviceId::size()) { return false; }
    out->readFrom(&data_[pos_]);
    pos_ += NetworkDeviceId::size();
    return true;
  }

  // Reads a time that was in the past from a uint32_t duration in milliseconds.
  bool ReadTimeSinceMs32(Microseconds* out, OptionalMicroseconds receiptTime = std::nullopt) {
    uint32_t deltaMs32;
    if (!ReadUint32(&deltaMs32)) { return false; }
    Microseconds receiptTimeUs = receiptTime.value_or(timeMicros());
    *out = receiptTimeUs - MillisecondsToMicroseconds(deltaMs32);
    return true;
  }

  // Reads a time that was in the past from a uint16_t duration in milliseconds.
  bool ReadTimeSinceMs16(Microseconds* out, OptionalMicroseconds receiptTime = std::nullopt) {
    uint16_t deltaMs16;
    if (!ReadUint16(&deltaMs16)) { return false; }
    Microseconds receiptTimeUs = receiptTime.value_or(timeMicros());
    *out = receiptTimeUs - MillisecondsToMicroseconds(deltaMs16);
    return true;
  }

 private:
  const uint8_t* data_;
  const size_t size_;
  size_t pos_ = 0;
};

}  // namespace jazzlights

#endif  // JL_PROTOCOL_READER_H
