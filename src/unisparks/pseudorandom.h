#ifndef UNISPARKS_PredictableRandom_H
#define UNISPARKS_PredictableRandom_H

#include <cstddef>
#include <cstdint>

#include "unisparks/frame.hpp"

namespace unisparks {

// Shared implementation, only meant to be used by subclasses.
class Random {
 public:
  virtual uint8_t GetRandomByte() = 0;
  virtual uint32_t GetRandom32bits() = 0;
  virtual void GetRandomBytes(void* buffer, size_t length) = 0;
  int32_t GetRandomNumberBetween(int32_t min, int32_t max);
 protected:
  Random() = default;
};

// Used to provide random-seeming bytes that are guaranteed to be the same
// during two separate invocations if they are reset with the same value
// and the ordering of calls remains the same.
class PredictableRandom : public Random {
 public:
  PredictableRandom();
  void ResetWithFrameStart(const Frame& frame, const char* label);
  void ResetWithFrameTime(const Frame& frame, const char* label);
  uint8_t GetRandomByte() override;
  uint32_t GetRandom32bits() override;
  void GetRandomBytes(void* buffer, size_t length) override;
 private:
  void ResetWithPatternTime(PatternBits pattern, Milliseconds elapsedTime, const char* label);
  void Reset();
  void IngestByte(uint8_t b);
  void IngestLabel(const char* label);
  void Ingest32bits(uint32_t i);
  void GenerateNextState();
  uint64_t state_;
  uint8_t numUsedStateBytes_;
};

// Provides the most unpredictable randomness that the underlying system can provide.
class UnpredictableRandom : public Random {
 public:
  static uint8_t GetByte();
  static uint32_t Get32bits();
  static void GetBytes(void* buffer, size_t length);
  static int32_t GetNumberBetween(int32_t min, int32_t max);
 private:
  UnpredictableRandom() = default;
  uint8_t GetRandomByte() override;
  uint32_t GetRandom32bits() override;
  void GetRandomBytes(void* buffer, size_t length) override;
};

}  // namespace unisparks

#endif  // UNISPARKS_PredictableRandom_H