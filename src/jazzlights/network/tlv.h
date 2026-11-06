#ifndef JL_NETWORK_TLV_H
#define JL_NETWORK_TLV_H

#include <cstddef>
#include <cstdint>

namespace jazzlights {

// The TLV encoding used by the secure channel: Type (varint) || Length (varint) || Value (Length bytes). Varints are
// QUIC variable-length integers (RFC 9000, section 16). This code has no dependencies, so it is also compiled natively.
//
// Lengths of TLVs and of their Values, and offsets within them, are uint64_t because that's what a varint can hold.
// Only sizes of buffers in memory are size_t.
//
// The maximum Length depends on where a TLV is, so callers of ParseTlvHeader() choose it. Fields in a container are
// only limited by it.

inline constexpr uint64_t kMaxVarint = (uint64_t{1} << 62) - 1;
inline constexpr size_t kMaxVarintSize = 8;
inline constexpr size_t kMaxTlvHeaderSize = 2 * kMaxVarintSize;

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
size_t ReadVarint(const uint8_t* data, uint64_t length, uint64_t* value);

enum class TlvHeaderStatus {
  kComplete,
  kNeedMoreData,
  kInvalid,  // The Length is over the maximum.
};

struct TlvHeader {
  uint64_t type = 0;
  uint64_t length = 0;
  size_t headerSize = 0;
};

// Parses a Type and Length from the start of `data`. The Value might not have arrived yet. A Length over `maxLength` is
// invalid, as long as the Type and Length are complete.
TlvHeaderStatus ParseTlvHeader(const uint8_t* data, uint64_t length, uint64_t maxLength, TlvHeader* header);

// Iterates over the TLVs of a complete container. Unknown types are up to the caller to skip.
class TlvReader {
 public:
  TlvReader(const uint8_t* data, uint64_t length) : data_(data), length_(length) {}

  // Moves to the next TLV. Returns false at the end of the container, or if it is malformed, see failed().
  bool Next();

  bool failed() const { return failed_; }
  uint64_t type() const { return header_.type; }
  const uint8_t* value() const { return data_ + offset_ + header_.headerSize; }
  uint64_t length() const { return header_.length; }
  // The complete TLV, including its Type and Length exactly as received.
  const uint8_t* tlv() const { return data_ + offset_; }
  uint64_t tlvLength() const { return header_.headerSize + header_.length; }

 private:
  const uint8_t* data_;
  uint64_t length_;
  uint64_t offset_ = 0;
  uint64_t next_ = 0;
  TlvHeader header_;
  bool failed_ = false;
};

// Parses an Integer: a varint that fills `length` exactly.
bool ParseTlvInteger(const uint8_t* value, uint64_t length, uint64_t* integer);

// Describes one field that a container can hold, and receives what was parsed.
struct TlvField {
  enum class Kind {
    kBytes,
    kInteger,
    kFlag,
  };
  static constexpr uint64_t kAnyLength = UINT64_MAX;

  TlvField(uint64_t type, Kind kind, bool required, uint64_t exactLength = kAnyLength)
      : type(type), kind(kind), required(required), exactLength(exactLength) {}

  // Inputs.
  uint64_t type;
  Kind kind;
  bool required;
  uint64_t exactLength;  // Only for kBytes.

  // Outputs.
  bool present = false;
  const uint8_t* value = nullptr;  // Only for kBytes.
  uint64_t length = 0;             // Only for kBytes.
  uint64_t integer = 0;            // Only for kInteger.
  const uint8_t* tlv = nullptr;    // The complete TLV, including its Type and Length exactly as received.
  uint64_t tlvLength = 0;
};

// Parses a Fields value. Unknown types are skipped, and fields can come in any order. Returns false, which must close
// the connection, if the container is malformed or if a field is repeated, the wrong size or missing.
bool ParseTlvFields(const uint8_t* data, uint64_t length, TlvField* fields, size_t numFields);

// Writes TLVs into a caller-provided buffer, using the shortest varints.
class TlvWriter {
 public:
  TlvWriter(uint8_t* buffer, size_t capacity) : buffer_(buffer), capacity_(capacity) {}

  void WriteBytes(uint64_t type, const uint8_t* value, uint64_t length);
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
