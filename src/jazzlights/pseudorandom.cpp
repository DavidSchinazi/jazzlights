#include "jazzlights/pseudorandom.h"

#include <limits>

#if defined(linux) || defined(__linux) || defined(__linux__)
#  if __GLIBC_PREREQ(2, 25)
#    include <sys/random.h>
#  else
#    include <fcntl.h>
#    include <unistd.h>
#  endif
#elif defined(ESP32)
#  include "esp_system.h"
#endif

#include "jazzlights/util/log.hpp"

namespace jazzlights {
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

int32_t Random::GetRandomNumberBetween(int32_t min, int32_t max) {
  if (min == max) { return min; }
  if (max == std::numeric_limits<int32_t>::max() &&
      min == std::numeric_limits<int32_t>::min()) {
    uint32_t rand32u = GetRandom32bits();
    if (rand32u <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
      return rand32u;
    } else {
      return static_cast<int32_t>(static_cast<int64_t>(rand32u) - (1LL << 32));
    }
  }
  const uint32_t numBins = max - min + 1;
  const uint32_t binSize = std::numeric_limits<uint32_t>::max() / numBins;
  const uint32_t defect = std::numeric_limits<uint32_t>::max() % numBins;
  uint32_t rand32;
  do {
   rand32 = GetRandom32bits();
  } while (std::numeric_limits<uint32_t>::max() - defect <= rand32);
  rand32 /= binSize;
  return min + static_cast<int32_t>(rand32);
}

double Random::GetRandomDoubleBetween(double min, double max) {
  constexpr int32_t kRandomGranularity = 10000;
  const double d = max - min;
  if (d < 0.000001) { return min; }
  return min + GetRandomNumberBetween(0, kRandomGranularity) * d / kRandomGranularity;
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

void PredictableRandom::ResetWithPatternTime(PatternBits pattern, Milliseconds elapsedTime, const char* label) {
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
uint8_t UnpredictableRandom::GetRandomByte() {
  return GetRandom32bits() & 0xFF;
}

uint32_t UnpredictableRandom::GetRandom32bits() {
#if defined(__APPLE__) || defined(linux) || defined(__linux) || defined(__linux__)
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
#if defined(__APPLE__)
  arc4random_buf(buffer, length);
#elif defined(linux) || defined(__linux) || defined(__linux__)
#  if __GLIBC_PREREQ(2, 25)
  (void)getrandom(buffer, length, /*flags=*/0);
#  else
  static int randFd = open("/dev/urandom", O_RDONLY);
  (void)read(randFd, buffer, length);
#  endif
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
  return UnpredictableRandom().GetRandomByte();
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
int32_t UnpredictableRandom::GetNumberBetween(int32_t min, int32_t max) {
  return UnpredictableRandom().GetRandomNumberBetween(min, max);
}

// static
double UnpredictableRandom::GetDoubleBetween(double min, double max) {
  return UnpredictableRandom().GetRandomDoubleBetween(min, max);
}

}  // namespace jazzlights
