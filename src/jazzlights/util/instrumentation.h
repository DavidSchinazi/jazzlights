#include "jazzlights/config.h"
#include "jazzlights/util/time.h"

#ifndef JL_INSTRUMENTATION_H
#define JL_INSTRUMENTATION_H

namespace jazzlights {

#if JL_INSTRUMENTATION || JL_TIMING
void printInstrumentationInfo(Milliseconds currentTime);
#else   // JL_INSTRUMENTATION || JL_TIMING
inline void printInstrumentationInfo(Milliseconds /*currentTime*/) {}
#endif  // JL_INSTRUMENTATION || JL_TIMING

#if JL_TIMING

#define ALL_TIME_POINTS \
  X(LoopStart)          \
  X(Core2)              \
  X(Buttons)            \
  X(Bluetooth)          \
  X(Player)             \
  X(Brightness)         \
  X(MainLED)            \
  X(SecondLED)          \
  X(NumTimePoints)  // Leave this as last member.

enum TimePoint {
#define X(v) k##v,
  ALL_TIME_POINTS
#undef X
};

void saveTimePoint(TimePoint timePoint);

void ledWriteStart();
void ledWriteEnd();

#define SAVE_TIME_POINT(v) saveTimePoint(k##v)

#define ALL_COUNT_POINTS \
  X(LedPrintLoop)        \
  X(LedPrintSend)        \
  X(NumCountPoints)  // Leave this as last member.

enum CountPoint {
#define X(v) k##v,
  ALL_COUNT_POINTS
#undef X
};

void saveCountPoint(CountPoint countPoint);

#define SAVE_COUNT_POINT(v) saveCountPoint(k##v)

#else  // JL_TIMING

#define SAVE_TIME_POINT(v) \
  do {                     \
  } while (false)
inline void ledWriteStart() {}
inline void ledWriteEnd() {}

#define SAVE_COUNT_POINT(v) \
  do {                      \
  } while (false)

#endif  // JL_TIMING

}  // namespace jazzlights

#endif  // JL_INSTRUMENTATION_H
