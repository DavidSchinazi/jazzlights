#ifndef JL_PROTOCOL_WRITER_H
#define JL_PROTOCOL_WRITER_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#include "jazzlights/protocol/wire_types.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

// Returns whether encoding a duration as milliseconds would not fit in a uint32_t.
inline bool DurationMs32Overflows(Microseconds durationUs) {
  const Microseconds durationMs = durationUs / kMicrosecondsPerMillisecond;
  return durationMs < 0 || durationMs > static_cast<Microseconds>(std::numeric_limits<uint32_t>::max());
}

// Returns whether a time is far enough in the past that the milliseconds elapsed since would not fit in a uint32_t.
inline bool TimeSinceMs32Overflows(Microseconds pastTime) { return DurationMs32Overflows(TimeMicros() - pastTime); }

class ProtocolWriter {
 public:
  explicit ProtocolWriter(uint8_t* data, size_t size) : data_(data), size_(size) {}
  size_t LengthWritten() const { return pos_; }

  bool WriteUint8(uint8_t in) {
    if (sizeof(in) > size_ || pos_ > size_ - sizeof(in)) { return false; }
    data_[pos_] = in;
    pos_ += sizeof(in);
    return true;
  }

  bool WriteUint16(uint16_t in) {
    if (sizeof(in) > size_ || pos_ > size_ - sizeof(in)) { return false; }
    data_[pos_] = static_cast<uint8_t>((in & 0xFF00) >> 8);
    data_[pos_ + 1] = static_cast<uint8_t>((in & 0x00FF));
    pos_ += sizeof(in);
    return true;
  }

  bool WriteUint32(uint32_t in) {
    if (sizeof(in) > size_ || pos_ > size_ - sizeof(in)) { return false; }
    data_[pos_] = static_cast<uint8_t>((in & 0xFF000000) >> 24);
    data_[pos_ + 1] = static_cast<uint8_t>((in & 0x00FF0000) >> 16);
    data_[pos_ + 2] = static_cast<uint8_t>((in & 0x0000FF00) >> 8);
    data_[pos_ + 3] = static_cast<uint8_t>((in & 0x000000FF));
    pos_ += sizeof(in);
    return true;
  }

  // Uses two's complement.
  bool WriteInt32(int32_t in) {
    uint32_t uin;
    if (in >= 0) {
      uin = static_cast<uint32_t>(in);
    } else {
      uin = 0xFFFFFFFFU - static_cast<uint32_t>(-(in + 1));
    }
    return WriteUint32(uin);
  }

  bool WriteNetworkDeviceId(const NetworkDeviceId& in) {
    if (NetworkDeviceId::size() > size_ || pos_ > size_ - NetworkDeviceId::size()) { return false; }
    in.WriteTo(&data_[pos_]);
    pos_ += NetworkDeviceId::size();
    return true;
  }

  // Writes a point in time in the past as a 32-bit count of milliseconds elapsed between it and now, pairing with
  // ProtocolReader::ReadTimeSinceMs32(). Writes nothing, and still returns true, if `t` is empty or if that many
  // elapsed milliseconds wouldn't fit in a uint32_t (see TimeSinceMs32Overflows()) — callers that need to track
  // whether a value was actually written (e.g. to set a presence flag) should check TimeSinceMs32Overflows()
  // themselves before calling this. Returns false only if writing an actual value failed.
  bool WriteOptionalTimeSinceMs32(OptionalMicroseconds epoch, OptionalMicroseconds sendTime = std::nullopt) {
    if (!epoch || TimeSinceMs32Overflows(*epoch)) { return true; }
    Microseconds sendTimeUs = sendTime.value_or(TimeMicros());
    return WriteUint32(static_cast<uint32_t>((sendTimeUs - *epoch) / kMicrosecondsPerMillisecond));
  }

  // Writes a duration as a 32-bit count of milliseconds. Writes nothing, and still returns true, if `d` is empty
  // or if that many milliseconds wouldn't fit in a uint32_t (see DurationMs32Overflows()) — callers that need to
  // track whether a value was actually written (e.g. to set a presence flag) should check DurationMs32Overflows()
  // themselves before calling this. Returns false only if writing an actual value failed.
  bool WriteOptionalDurationMs32(OptionalMicroseconds d) {
    if (!d || DurationMs32Overflows(*d)) { return true; }
    return WriteUint32(static_cast<uint32_t>(*d / kMicrosecondsPerMillisecond));
  }

  // Writes the time since `epoch` as a uint16_t in milliseconds. Clamps to 0x0000 or 0xFFFF if value is out of bounds.
  bool WriteTimeSinceMs16(Microseconds epoch, OptionalMicroseconds sendTime = std::nullopt) {
    Microseconds sendTimeUs = sendTime.value_or(TimeMicros());
    uint16_t durationMs16;
    if (sendTimeUs <= epoch) {
      durationMs16 = 0;
    } else {
      int64_t timeSinceMs = (sendTimeUs - epoch) / kMicrosecondsPerMillisecond;
      if (timeSinceMs >= static_cast<int64_t>(std::numeric_limits<uint16_t>::max())) {
        durationMs16 = std::numeric_limits<uint16_t>::max();
      } else {
        durationMs16 = static_cast<uint16_t>(timeSinceMs);
      }
    }
    return WriteUint16(durationMs16);
  }

 private:
  uint8_t* data_;
  const size_t size_;
  size_t pos_ = 0;
};

}  // namespace jazzlights

#endif  // JL_PROTOCOL_WRITER_H
