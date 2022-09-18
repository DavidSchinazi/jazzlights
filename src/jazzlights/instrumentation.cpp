#include "jazzlights/instrumentation.h"

#if JL_INSTRUMENTATION

#include "jazzlights/util/log.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace jazzlights {

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

void saveTimePoint(TimePoint timePoint) {
  TimePointData& thisTimePointData = gTimePointDatas[timePoint];
  const TimePointData& prevTimePointData = timePoint > 0 ? gTimePointDatas[timePoint - 1] : gTimePointDatas[kNumTimePoints - 1];
  thisTimePointData.lastSavedTime = esp_timer_get_time();
  if (prevTimePointData.lastSavedTime >= 0) {
    thisTimePointData.sumTimes += thisTimePointData.lastSavedTime - prevTimePointData.lastSavedTime;
  }
}

void printInstrumentationInfo(Milliseconds currentTime) {
  static Milliseconds lastInstrumentationLog = -1;
  static constexpr Milliseconds kInstrumentationPeriod = 5000;
  if (lastInstrumentationLog >= 0 &&
      currentTime - lastInstrumentationLog < kInstrumentationPeriod) {
    // Ignore any request to print more than once every 5s.
    return;
  }
  char vTaskInfoStr[2048];
  vTaskGetRunTimeStats(vTaskInfoStr);
  vTaskInfoStr[sizeof(vTaskInfoStr) - 1] = '\0';
  info("%u INSTRUMENTATION:\n%s", currentTime, vTaskInfoStr);
  lastInstrumentationLog = currentTime;
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
}

}  // namespace jazzlights

#endif  // JL_INSTRUMENTATION
