#include "unisparks/pseudorandom.h"

#include <limits>

#if defined(linux) || defined(__linux) || defined(__linux__)
#  include <sys/random.h>
#elif defined(ESP32)
#  include "esp_system.h"
#endif

namespace unisparks {
namespace {

// Implementation of the 64bit variant of FNV1a.
// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
constexpr uint64_t kFNV1a64Prime = 0x100000001B3;
constexpr uint64_t kFNV1a64OffsetBasis = 0xCBF29CE484222325;
inline uint64_t NextFnv1a64Value(uint64_t hash, uint8_t b) {
  return (hash ^ b) * kFNV1a64Prime;
}

// Implementation of the 64bit variant of xorshift*.
// https://en.wikipedia.org/wiki/Xorshift#xorshift*
inline uint64_t NextXorShift64StarValue(uint64_t x) {
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  return x * 0x2545F4914F6CDD1DULL;
}

}  // namespace

uint32_t Random::GetRandomNumberBetween(uint32_t min, uint32_t max) {
  if (max == std::numeric_limits<uint32_t>::max() && min == 0) {
    return GetRandom32bits();
  }
  const uint32_t numBins = max - min + 1;
  const uint32_t binSize = std::numeric_limits<uint32_t>::max() / numBins;
  const uint32_t defect = std::numeric_limits<uint32_t>::max() % numBins;
  uint32_t rand32;
  do {
   rand32 = GetRandom32bits();
  } while (std::numeric_limits<uint32_t>::max() - defect <= rand32);
  return min + rand32 / binSize;
}

void PredictableRandom::IngestByte(uint8_t b) {
  state_ = NextFnv1a64Value(state_, b);
}

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


PredictableRandom::PredictableRandom() {
  Reset();
}

void PredictableRandom::Reset() {
  state_ = kFNV1a64OffsetBasis;
  numUsedStateBytes_ = 0;
}

void PredictableRandom::ResetWithFrame(const Frame& frame, const char* label) {
  Reset();
  IngestLabel(label);
  Ingest32bits(frame.pattern);
  IngestLabel(label);
  Ingest32bits(frame.time);
  IngestLabel(label);
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
    memcpy(buffer8, &state_ + numUsedStateBytes_, amountToCopy);
    buffer8 += amountToCopy;
    length -= amountToCopy;
    GenerateNextState();
  } while (true);
  memcpy(buffer8, &state_ + numUsedStateBytes_, length);
  numUsedStateBytes_ += length;
}
uint8_t UnpredictableRandom::GetRandomByte() {
  return GetRandom32bits() & 0xFF;
}

uint32_t UnpredictableRandom::GetRandom32bits() {
#if defined(__APPLE___) || defined(linux) || defined(__linux) || defined(__linux__)
  uint32_t result;
  GetRandomBytes(&result, sizeof(result));
  return result;
#elif defined(ESP32)
  return esp_random();
#elif defined(ESP8266)
  const uint32_t randOne = static_cast<uint32_t>(rand());
  const uint32_t randTwo = static_cast<uint32_t>(rand());
  return randOne ^ ((randTwo << 16) | (randTwo >> 16));
#else
#  error "Unsupported platform"
#endif
}

void UnpredictableRandom::GetRandomBytes(void* buffer, size_t length) {
#if defined(__APPLE___)
  arc4random_buf(buffer, length);
#elif defined(linux) || defined(__linux) || defined(__linux__)
  (void)getrandom(buffer, length, /*flags=*/0);
#else
  uint8_t* buffer8 = reinterpret_cast<uint8_t*>(buffer);
  while (length >= sizeof(uint32_t)) {
    uint32_t rand32 = GetRandom32bits();
    memcpy(buffer8, &rand32, sizeof(rand32));
    buffer8 += sizeof(rand32);
    length -= sizeof(rand32);
  }
  if (length != 0) {
    uint32_t rand32 = GetRandom32bits();
    memcpy(buffer8, &rand32, length);
  }
#endif
}

// static
uint8_t UnpredictableRandom::GetByte() {
  return UnpredictableRandom().GetByte();
}

// static
uint32_t UnpredictableRandom::Get32bits() {
  return UnpredictableRandom().GetRandom32bits();
}

// static
void UnpredictableRandom::GetBytes(void* buffer, size_t length) {
  return UnpredictableRandom().GetRandomBytes(buffer, length);
}

// static
uint32_t UnpredictableRandom::GetNumberBetween(uint32_t min, uint32_t max) {
  return UnpredictableRandom().GetRandomNumberBetween(min, max);
}

}  // namespace unisparks
