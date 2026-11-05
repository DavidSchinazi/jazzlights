#ifndef JL_NETWORK_TLV_H
#define JL_NETWORK_TLV_H

#include <cstddef>
#include <cstdint>

namespace jazzlights {

// The TLV encoding used by the secure channel: Type (varint) || Length (varint) || Value (Length bytes). Varints are
// QUIC variable-length integers (RFC 9000, section 16). This code has no dependencies, so it is also compiled natively.

inline constexpr uint64_t kMaxTLVLength = 16384;
inline constexpr uint64_t kMaxVarint = (uint64_t{1} << 62) - 1;
inline constexpr size_t kMaxVarintSize = 8;
inline constexpr size_t kMaxTLVHeaderSize = 2 * kMaxVarintSize;

// Returns the size of the shortest encoding of `value`, which must be at most kMaxVarint.
constexpr size_t VarintSize(uint64_t value) {
  if (value < (uint64_t{1} << 6)) { return 1; }
  if (value < (uint64_t{1} << 14)) { return 2; }
  if (value < (uint64_t{1} << 30)) { return 4; }
  return 8;
}

// Writes the shortest encoding of `value`, which must be at most kMaxVarint, and returns its size. `out` must have room
// for VarintSize(value) bytes.
size_t WriteVarint(uint64_t value, uint8_t* out);

// Reads a varint of any size. Returns the number of bytes it occupies, or 0 if it is cut off.
size_t ReadVarint(const uint8_t* data, size_t length, uint64_t* value);

enum class TLVHeaderStatus {
  kComplete,
  kNeedMoreData,
  kInvalid,  // The Length is over kMaxTLVLength.
};

struct TLVHeader {
  uint64_t type = 0;
  size_t length = 0;
  size_t headerSize = 0;
};

// Parses a Type and Length from the start of `data`. The Value might not have arrived yet.
TLVHeaderStatus ParseTLVHeader(const uint8_t* data, size_t length, TLVHeader* header);

// Iterates over the TLVs of a complete container. Unknown types are up to the caller to skip.
class TLVReader {
 public:
  TLVReader(const uint8_t* data, size_t length) : data_(data), length_(length) {}

  // Moves to the next TLV. Returns false at the end of the container, or if it is malformed, see failed().
  bool Next();

  bool failed() const { return failed_; }
  uint64_t type() const { return header_.type; }
  const uint8_t* value() const { return data_ + offset_ + header_.headerSize; }
  size_t length() const { return header_.length; }
  // The complete TLV, including its Type and Length exactly as received.
  const uint8_t* tlv() const { return data_ + offset_; }
  size_t tlvLength() const { return header_.headerSize + header_.length; }

 private:
  const uint8_t* data_;
  size_t length_;
  size_t offset_ = 0;
  size_t next_ = 0;
  TLVHeader header_;
  bool failed_ = false;
};

// Parses an Integer: a varint that fills `length` exactly.
bool ParseTLVInteger(const uint8_t* value, size_t length, uint64_t* integer);

// Describes one field that a container can hold, and receives what was parsed.
struct TLVField {
  enum class Kind {
    kBytes,
    kInteger,
    kFlag,
  };
  static constexpr size_t kAnyLength = SIZE_MAX;

  TLVField(uint64_t type, Kind kind, bool required, size_t exactLength = kAnyLength)
      : type(type), kind(kind), required(required), exactLength(exactLength) {}

  // Inputs.
  uint64_t type;
  Kind kind;
  bool required;
  size_t exactLength;  // Only for kBytes.

  // Outputs.
  bool present = false;
  const uint8_t* value = nullptr;  // Only for kBytes.
  size_t length = 0;               // Only for kBytes.
  uint64_t integer = 0;            // Only for kInteger.
  const uint8_t* tlv = nullptr;    // The complete TLV, including its Type and Length exactly as received.
  size_t tlvLength = 0;
};

// Parses a Fields value. Unknown types are skipped, and fields can come in any order. Returns false, which must close
// the connection, if the container is malformed or if a field is repeated, the wrong size or missing.
bool ParseTLVFields(const uint8_t* data, size_t length, TLVField* fields, size_t numFields);

// Writes TLVs into a caller-provided buffer, using the shortest varints.
class TLVWriter {
 public:
  TLVWriter(uint8_t* buffer, size_t capacity) : buffer_(buffer), capacity_(capacity) {}

  void WriteBytes(uint64_t type, const uint8_t* value, size_t length);
  void WriteInteger(uint64_t type, uint64_t integer);
  void WriteFlag(uint64_t type) { WriteBytes(type, nullptr, 0); }

  // Whether something didn't fit, in which case the contents must not be used.
  bool failed() const { return failed_; }
  size_t size() const { return size_; }

 private:
  uint8_t* buffer_;
  size_t capacity_;
  size_t size_ = 0;
  bool failed_ = false;
};

}  // namespace jazzlights

#endif  // JL_NETWORK_TLV_H
