#include "jazzlights/util/pseudorandom.h"

#include <cstdlib>
#include <cstring>
#include <limits>

#if defined(linux) || defined(__linux) || defined(__linux__)
#if __GLIBC_PREREQ(2, 25)
#include <sys/random.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
#elif defined(ESP32)
#include <esp_random.h>
#endif

namespace jazzlights {

int32_t Random::GetRandomNumberBetween(int32_t min, int32_t max) {
  if (min == max) { return min; }
  if (max == std::numeric_limits<int32_t>::max() && min == std::numeric_limits<int32_t>::min()) {
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
  do { rand32 = GetRandom32bits(); } while (std::numeric_limits<uint32_t>::max() - defect <= rand32);
  rand32 /= binSize;
  return min + static_cast<int32_t>(rand32);
}

double Random::GetRandomDoubleBetween(double min, double max) {
  constexpr int32_t kRandomGranularity = 10000;
  const double d = max - min;
  if (d < 0.000001) { return min; }
  return min + GetRandomNumberBetween(0, kRandomGranularity) * d / kRandomGranularity;
}

uint8_t UnpredictableRandom::GetRandomByte() { return GetRandom32bits() & 0xFF; }

uint32_t UnpredictableRandom::GetRandom32bits() {
#if defined(__APPLE__) || defined(linux) || defined(__linux) || defined(__linux__)
  uint32_t result;
  GetRandomBytes(&result, sizeof(result));
  return result;
#elif defined(ESP32)
  return esp_random();
#else
#error "Unsupported platform"
#endif
}

void UnpredictableRandom::GetRandomBytes(void* buffer, size_t length) {
#if defined(__APPLE__)
  arc4random_buf(buffer, length);
#elif defined(linux) || defined(__linux) || defined(__linux__)
#if __GLIBC_PREREQ(2, 25)
  (void)getrandom(buffer, length, /*flags=*/0);
#else
  static int randFd = open("/dev/urandom", O_RDONLY);
  (void)read(randFd, buffer, length);
#endif
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
uint8_t UnpredictableRandom::GetByte() { return UnpredictableRandom().GetRandomByte(); }

// static
uint32_t UnpredictableRandom::Get32bits() { return UnpredictableRandom().GetRandom32bits(); }

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
