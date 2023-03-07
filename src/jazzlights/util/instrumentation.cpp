#include "jazzlights/util/instrumentation.h"

#if JL_INSTRUMENTATION || JL_TIMING
#include <freertos/FreeRTOS.h>

#include <cstddef>

#include "jazzlights/util/log.h"
#endif  // JL_INSTRUMENTATION || JL_TIMING

#if JL_INSTRUMENTATION
#include <freertos/task.h>
#endif  // JL_INSTRUMENTATION

#if JL_TIMING
#include <limits>
#endif  // JL_TIMING

namespace jazzlights {

#if JL_TIMING

namespace {

const char* TimePointToString(TimePoint timePoint) {
  switch (timePoint) {
#define X(v) \
  case k##v: return #v;
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

void printAndClearTimePoints() {
  int64_t totalTimePointsSum = 0;
  for (size_t i = 0; i < kNumTimePoints; i++) { totalTimePointsSum += gTimePointDatas[i].sumTimes; }
  const int64_t minPercentOffset = totalTimePointsSum / 200;
  for (size_t i = 0; i < kNumTimePoints; i++) {
    jll_info("%12s: %2lld%% %8lld", TimePointToString(static_cast<TimePoint>(i)),
             (gTimePointDatas[i].sumTimes * 100 + minPercentOffset) / totalTimePointsSum, gTimePointDatas[i].sumTimes);
  }
  for (size_t i = 0; i < kNumTimePoints; i++) { gTimePointDatas[i] = TimePointData(); }
}

const char* CountPointToString(CountPoint countPoint) {
  switch (countPoint) {
#define X(v) \
  case k##v: return #v;
    ALL_COUNT_POINTS
#undef X
  }
  return "Unknown";
}

int64_t gLastCountPointPrint = -1;
uint64_t gCountPointDatas[kNumCountPoints];

void printAndClearCountPoints() {
  int64_t curTime = esp_timer_get_time();
  if (gLastCountPointPrint >= 0) {
    const int64_t period = curTime - gLastCountPointPrint;
    for (size_t i = 0; i < kNumCountPoints; i++) {
      jll_info("%12s: %f counts/s", CountPointToString(static_cast<CountPoint>(i)),
               1000000.0 * gCountPointDatas[i] / period);
    }
  }
  gLastCountPointPrint = curTime;
  for (size_t i = 0; i < kNumCountPoints; i++) { gCountPointDatas[i] = 0; }
}

}  // namespace

int64_t gNumLedWrites = 0;

void saveTimePoint(TimePoint timePoint) {
  TimePointData& thisTimePointData = gTimePointDatas[timePoint];
  const TimePointData& prevTimePointData =
      timePoint > 0 ? gTimePointDatas[timePoint - 1] : gTimePointDatas[kNumTimePoints - 1];
  thisTimePointData.lastSavedTime = esp_timer_get_time();
  if (prevTimePointData.lastSavedTime >= 0) {
    thisTimePointData.sumTimes += thisTimePointData.lastSavedTime - prevTimePointData.lastSavedTime;
  }
  if (timePoint == kMainLED) { gNumLedWrites++; }
}

void saveCountPoint(CountPoint countPoint) { gCountPointDatas[countPoint]++; }

int64_t gLastLedStart = 0;
int64_t gLedTimeSum = 0;
int64_t gLedTimeCount = 0;
int64_t gLedTimeMin = std::numeric_limits<int64_t>::max();
int64_t gLedTimeMax = -1;

void ledWriteStart() { gLastLedStart = esp_timer_get_time(); }

void ledWriteEnd() {
  const int64_t ledTime = esp_timer_get_time() - gLastLedStart;
  gLedTimeSum += ledTime;
  gLedTimeCount++;
  if (ledTime < gLedTimeMin) { gLedTimeMin = ledTime; }
  if (ledTime > gLedTimeMax) { gLedTimeMax = ledTime; }
}

#endif  // JL_TIMING

#if JL_INSTRUMENTATION

const char* TaskStateToString(eTaskState taskState) {
  switch (taskState) {
#define TASK_STATE_CASE_RETURN(ts) \
  case e##ts: return #ts
    case eRunning: return "Running  ";
    case eReady: return "Ready    ";
    case eBlocked: return "Blocked  ";
    case eSuspended: return "Suspended";
    case eDeleted: return "Deleted  ";
    case eInvalid: return "Invalid  ";
#undef TASK_STATE_CASE_RETURN
  }
  return "Unknown  ";
}

#endif  // JL_INSTRUMENTATION

#if JL_INSTRUMENTATION || JL_TIMING

void printInstrumentationInfo(Milliseconds currentTime) {
  static Milliseconds lastInstrumentationLog = -1;
  static constexpr Milliseconds kInstrumentationPeriod = 5000;
  if (lastInstrumentationLog >= 0 && currentTime - lastInstrumentationLog < kInstrumentationPeriod) {
    // Ignore any request to print more than once every 5s.
    return;
  }
#if JL_INSTRUMENTATION
  UBaseType_t numTasks = uxTaskGetNumberOfTasks();
  // Add 10 in case some tasks get created while this is getting executed.
  TaskStatus_t* tastStatuses = reinterpret_cast<TaskStatus_t*>(calloc(numTasks + 10, sizeof(TaskStatus_t)));
  uint32_t totalRuntime = 0;
  numTasks = uxTaskGetSystemState(tastStatuses, numTasks, &totalRuntime);
  jll_info("%u INSTRUMENTATION for %u tasks:", currentTime, numTasks);
  for (UBaseType_t i = 0; i < numTasks; i++) {
    const TaskStatus_t& ts = tastStatuses[i];
    uint32_t percentRuntime = 0;
    if (ts.ulRunTimeCounter > 0 && totalRuntime > 0) {
      percentRuntime = (ts.ulRunTimeCounter + (totalRuntime / 200)) / (totalRuntime / 100);
    }
    static_assert(configMAX_TASK_NAME_LEN == 16, "tweak format string");
    jll_info("%16s: num=%02u %s priority=current%02u/base%02u runtime=%09u=%02u%% core=%+d", ts.pcTaskName,
             ts.xTaskNumber, TaskStateToString(ts.eCurrentState), ts.uxCurrentPriority, ts.uxBasePriority,
             ts.ulRunTimeCounter, percentRuntime, (ts.xCoreID == 2147483647 ? -1 : ts.xCoreID));
  }
  free(tastStatuses);
#endif  // JL_INSTRUMENTATION
#if JL_TIMING
  printAndClearTimePoints();
  printAndClearCountPoints();
  jll_info("Wrote to LEDs %f times per second",
           static_cast<double>(gNumLedWrites) * 1000 / (currentTime - lastInstrumentationLog));
  gNumLedWrites = 0;
  lastInstrumentationLog = currentTime;
  if (gLedTimeCount > 0) {
    jll_info("LED data from %lld writes: min %lld average %lld max %lld (us)", gLedTimeCount, gLedTimeMin,
             gLedTimeSum / gLedTimeCount, gLedTimeMax);
  } else {
    jll_info("No LED timing data available");
  }
  gLedTimeCount = 0;
  gLedTimeSum = 0;
  gLedTimeMin = std::numeric_limits<int64_t>::max();
  gLedTimeMax = -1;
#endif  // JL_TIMING
}

#endif  // JL_INSTRUMENTATION || JL_TIMING

/* Here are some common tasks seen on an ATOM Matrix for future reference:
      loopTask: num=08 priority=01 core=+1
          IDLE: num=05 priority=00 core=+0
          IDLE: num=06 priority=00 core=+1
           tiT: num=15 priority=18 core=+0
          ipc0: num=01 priority=24 core=+0
          ipc1: num=02 priority=24 core=+1
arduino_events: num=17 priority=19 core=+1
       sys_evt: num=16 priority=20 core=+0
          wifi: num=18 priority=23 core=+0
     esp_timer: num=03 priority=22 core=+0
       FastLED: num=14 priority=24 core=+0
          hciT: num=12 priority=22 core=+0
  btController: num=10 priority=23 core=+0
      BTU_TASK: num=13 priority=20 core=+0
      BTC_TASK: num=11 priority=19 core=+0
       Tmr Svc: num=07 priority=01 core=+0
*/

}  // namespace jazzlights
