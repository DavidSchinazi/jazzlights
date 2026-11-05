#include "jazzlights/network/tlv.h"

#include <cstring>

namespace jazzlights {

size_t WriteVarint(uint64_t value, uint8_t* out) {
  const size_t size = VarintSize(value);
  for (size_t i = 0; i < size; i++) { out[i] = static_cast<uint8_t>(value >> (8 * (size - 1 - i))); }
  // The two most significant bits hold the base-2 logarithm of the size.
  out[0] |= static_cast<uint8_t>((size == 1 ? 0 : size == 2 ? 1 : size == 4 ? 2 : 3) << 6);
  return size;
}

size_t ReadVarint(const uint8_t* data, size_t length, uint64_t* value) {
  if (length == 0) { return 0; }
  const size_t size = size_t{1} << (data[0] >> 6);
  if (length < size) { return 0; }
  uint64_t result = data[0] & 0x3f;
  for (size_t i = 1; i < size; i++) { result = (result << 8) | data[i]; }
  *value = result;
  return size;
}

TLVHeaderStatus ParseTLVHeader(const uint8_t* data, size_t length, TLVHeader* header) {
  uint64_t type;
  const size_t typeSize = ReadVarint(data, length, &type);
  if (typeSize == 0) { return TLVHeaderStatus::kNeedMoreData; }
  uint64_t valueLength;
  const size_t lengthSize = ReadVarint(data + typeSize, length - typeSize, &valueLength);
  if (lengthSize == 0) { return TLVHeaderStatus::kNeedMoreData; }
  if (valueLength > kMaxTLVLength) { return TLVHeaderStatus::kInvalid; }
  header->type = type;
  header->length = static_cast<size_t>(valueLength);
  header->headerSize = typeSize + lengthSize;
  return TLVHeaderStatus::kComplete;
}

bool TLVReader::Next() {
  if (failed_) { return false; }
  offset_ = next_;
  if (offset_ == length_) { return false; }
  // A Type or Length that is cut off is past the end of the container, since nothing more can arrive.
  if (ParseTLVHeader(data_ + offset_, length_ - offset_, &header_) != TLVHeaderStatus::kComplete ||
      header_.length > length_ - offset_ - header_.headerSize) {
    failed_ = true;
    return false;
  }
  next_ = offset_ + header_.headerSize + header_.length;
  return true;
}

bool ParseTLVInteger(const uint8_t* value, size_t length, uint64_t* integer) {
  return length > 0 && ReadVarint(value, length, integer) == length;
}

bool ParseTLVFields(const uint8_t* data, size_t length, TLVField* fields, size_t numFields) {
  TLVReader reader(data, length);
  while (reader.Next()) {
    for (size_t i = 0; i < numFields; i++) {
      TLVField& field = fields[i];
      if (field.type != reader.type()) { continue; }
      if (field.present) { return false; }
      switch (field.kind) {
        case TLVField::Kind::kBytes:
          if (field.exactLength != TLVField::kAnyLength && field.exactLength != reader.length()) { return false; }
          field.value = reader.value();
          field.length = reader.length();
          break;
        case TLVField::Kind::kInteger:
          if (!ParseTLVInteger(reader.value(), reader.length(), &field.integer)) { return false; }
          break;
        case TLVField::Kind::kFlag:
          if (reader.length() != 0) { return false; }
          break;
      }
      field.present = true;
      field.tlv = reader.tlv();
      field.tlvLength = reader.tlvLength();
      break;
    }
  }
  if (reader.failed()) { return false; }
  for (size_t i = 0; i < numFields; i++) {
    if (fields[i].required && !fields[i].present) { return false; }
  }
  return true;
}

void TLVWriter::WriteBytes(uint64_t type, const uint8_t* value, size_t length) {
  const size_t headerSize = VarintSize(type) + VarintSize(length);
  if (failed_ || length > kMaxTLVLength || headerSize + length > capacity_ - size_) {
    failed_ = true;
    return;
  }
  size_ += WriteVarint(type, buffer_ + size_);
  size_ += WriteVarint(length, buffer_ + size_);
  if (length > 0) { memcpy(buffer_ + size_, value, length); }
  size_ += length;
}

void TLVWriter::WriteInteger(uint64_t type, uint64_t integer) {
  uint8_t value[kMaxVarintSize];
  WriteBytes(type, value, WriteVarint(integer, value));
}

}  // namespace jazzlights
