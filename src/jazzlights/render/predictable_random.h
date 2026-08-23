#ifndef JL_RENDER_PREDICTABLE_RANDOM_H
#define JL_RENDER_PREDICTABLE_RANDOM_H

#include <cstdint>

#include "jazzlights/render/frame.h"
#include "jazzlights/util/pseudorandom.h"
#include "jazzlights/util/types.h"

namespace jazzlights {

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
  void ResetWithPatternTime(PatternBits pattern, FrameTimeMs elapsedTime, const char* label);
  void Reset();
  void IngestByte(uint8_t b);
  void IngestLabel(const char* label);
  void Ingest32bits(uint32_t i);
  void GenerateNextState();
  uint64_t state_;
  uint8_t numUsedStateBytes_;
};

}  // namespace jazzlights

#endif  // JL_RENDER_PREDICTABLE_RANDOM_H
