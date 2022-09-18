#include "jazzlights/instrumentation.h"

#if JL_INSTRUMENTATION || JL_TIMING
#  include "jazzlights/util/log.h"
#  include <freertos/FreeRTOS.h>
#  include <cstddef>
#endif  // JL_INSTRUMENTATION || JL_TIMING

#if JL_INSTRUMENTATION
#  include <freertos/task.h>
#endif  // JL_INSTRUMENTATION

#if JL_TIMING
#  include <limits>
#endif  // JL_TIMING

namespace jazzlights {

#if JL_TIMING

namespace {

const char* TimePointToString(TimePoint timePoint) {
  switch (timePoint) {
#define X(v) case k ## v: return #v;
    ALL_TIME_POINTS
#undef X
  }
  return "Unknown";
}

struct TimePointData {
  int64_t lastSavedTime = -1;
  int64_t sumTimes = 0;
};

TimePointData gTimePointDatas[kNumTimePoints];

void clearTimePoints() {
  for (size_t i = 0; i < kNumTimePoints; i++) {
    gTimePointDatas[i] = TimePointData();
  }
}

}  // namespace

int64_t gNumLedWrites = 0;

void saveTimePoint(TimePoint timePoint) {
  TimePointData& thisTimePointData = gTimePointDatas[timePoint];
  const TimePointData& prevTimePointData = timePoint > 0 ? gTimePointDatas[timePoint - 1] : gTimePointDatas[kNumTimePoints - 1];
  thisTimePointData.lastSavedTime = esp_timer_get_time();
  if (prevTimePointData.lastSavedTime >= 0) {
    thisTimePointData.sumTimes += thisTimePointData.lastSavedTime - prevTimePointData.lastSavedTime;
  }
  if (timePoint == kMainLED) {
    gNumLedWrites++;
  }
}

int64_t gLastLedStart = 0;
int64_t gLedTimeSum = 0;
int64_t gLedTimeCount = 0;
int64_t gLedTimeMin = std::numeric_limits<int64_t>::max();
int64_t gLedTimeMax = -1;

void ledWriteStart() {
  gLastLedStart = esp_timer_get_time();
}

void ledWriteEnd() {
  const int64_t ledTime = esp_timer_get_time() - gLastLedStart;
  gLedTimeSum += ledTime;
  gLedTimeCount++;
  if (ledTime < gLedTimeMin) { gLedTimeMin = ledTime; }
  if (ledTime > gLedTimeMax) { gLedTimeMax = ledTime; }
}

#endif  // JL_TIMING

#if JL_INSTRUMENTATION || JL_TIMING

void printInstrumentationInfo(Milliseconds currentTime) {
  static Milliseconds lastInstrumentationLog = -1;
  static constexpr Milliseconds kInstrumentationPeriod = 5000;
  if (lastInstrumentationLog >= 0 &&
      currentTime - lastInstrumentationLog < kInstrumentationPeriod) {
    // Ignore any request to print more than once every 5s.
    return;
  }
#if JL_INSTRUMENTATION
  char vTaskInfoStr[2048];
  vTaskGetRunTimeStats(vTaskInfoStr);
  vTaskInfoStr[sizeof(vTaskInfoStr) - 1] = '\0';
  info("%u INSTRUMENTATION:\n%s", currentTime, vTaskInfoStr);
#endif  // JL_INSTRUMENTATION
#if JL_TIMING
  int64_t totalTimePointsSum = 0;
  for (size_t i = 0; i < kNumTimePoints; i++) {
    totalTimePointsSum += gTimePointDatas[i].sumTimes;
  }
  const int64_t minPercentOffset = totalTimePointsSum / 200;
  for (size_t i = 0; i < kNumTimePoints; i++) {
    info("%s: %lld %lld%%",
         TimePointToString(static_cast<TimePoint>(i)),
         gTimePointDatas[i].sumTimes,
         (gTimePointDatas[i].sumTimes * 100 + minPercentOffset) / totalTimePointsSum);
  }
  clearTimePoints();
  info("Wrote to LEDs %f times per second",
       static_cast<double>(gNumLedWrites) * 1000 / (currentTime - lastInstrumentationLog));
  gNumLedWrites = 0;
  lastInstrumentationLog = currentTime;
  if (gLedTimeCount > 0) {
    info("LED data from %lld writes: min %lld average %lld max %lld (us)",
        gLedTimeCount, gLedTimeMin, gLedTimeSum / gLedTimeCount, gLedTimeMax);
  } else {
    info("No LED data available");
  }
  gLedTimeCount = 0;
  gLedTimeSum = 0;
  gLedTimeMin = std::numeric_limits<int64_t>::max();
  gLedTimeMax = -1;
#endif  // JL_TIMING
}

#endif  // JL_INSTRUMENTATION || JL_TIMING

}  // namespace jazzlights
