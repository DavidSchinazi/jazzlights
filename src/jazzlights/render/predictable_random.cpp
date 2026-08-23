#include "jazzlights/render/predictable_random.h"

#include <cstring>

namespace jazzlights {
namespace {

// Implementation of the 64bit variant of FNV1a.
// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
constexpr uint64_t kFNV1a64Prime = 0x100000001B3;
constexpr uint64_t kFNV1a64OffsetBasis = 0xCBF29CE484222325;
constexpr uint64_t NextFnv1a64Value(uint64_t hash, uint8_t b) { return (hash ^ b) * kFNV1a64Prime; }

// Implementation of the 64bit variant of xorshift*.
// https://en.wikipedia.org/wiki/Xorshift#xorshift*
inline uint64_t NextXorShift64StarValue(uint64_t x) {
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  return x * 0x2545F4914F6CDD1DULL;
}

}  // namespace

void PredictableRandom::IngestByte(uint8_t b) { state_ = NextFnv1a64Value(state_, b); }

void PredictableRandom::IngestLabel(const char* label) {
  while (*label != '\0') {
    IngestByte(static_cast<uint8_t>(*label));
    label++;
  }
  IngestByte('\0');
}

void PredictableRandom::Ingest32bits(uint32_t i) {
  IngestByte(i >> 24);
  IngestByte((i >> 16) & 0xFF);
  IngestByte((i >> 8) & 0xFF);
  IngestByte(i & 0xFF);
}

PredictableRandom::PredictableRandom() { Reset(); }

void PredictableRandom::Reset() {
  state_ = kFNV1a64OffsetBasis;
  numUsedStateBytes_ = 0;
}

void PredictableRandom::ResetWithPatternTime(PatternBits pattern, FrameTimeMs elapsedTime, const char* label) {
  Reset();
  IngestLabel(label);
  Ingest32bits(pattern);
  IngestLabel(label);
  Ingest32bits(elapsedTime);
  IngestLabel(label);
}

void PredictableRandom::ResetWithFrameStart(const Frame& frame, const char* label) {
  ResetWithPatternTime(frame.pattern, 0, label);
}

void PredictableRandom::ResetWithFrameTime(const Frame& frame, const char* label) {
  ResetWithPatternTime(frame.pattern, frame.time, label);
}

void PredictableRandom::GenerateNextState() {
  state_ = NextXorShift64StarValue(state_);
  numUsedStateBytes_ = 0;
}

uint8_t PredictableRandom::GetRandomByte() {
  uint8_t result;
  GetRandomBytes(&result, sizeof(result));
  return result;
}

uint32_t PredictableRandom::GetRandom32bits() {
  uint32_t result;
  GetRandomBytes(&result, sizeof(result));
  return result;
}

void PredictableRandom::GetRandomBytes(void* buffer, size_t length) {
  uint8_t* buffer8 = reinterpret_cast<uint8_t*>(buffer);
  do {
    const uint8_t amountToCopy = sizeof(uint64_t) - numUsedStateBytes_;
    if (length < amountToCopy) { break; }
    memcpy(buffer8, reinterpret_cast<uint8_t*>(&state_) + numUsedStateBytes_, amountToCopy);
    buffer8 += amountToCopy;
    length -= amountToCopy;
    GenerateNextState();
  } while (true);
  memcpy(buffer8, reinterpret_cast<uint8_t*>(&state_) + numUsedStateBytes_, length);
  numUsedStateBytes_ += length;
}

}  // namespace jazzlights
