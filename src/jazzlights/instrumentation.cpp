#include "jazzlights/instrumentation.h"

#if JL_INSTRUMENTATION

#include "jazzlights/util/log.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace jazzlights {

void printInstrumentationInfo(Milliseconds currentTime) {
  static Milliseconds lastInstrumentationLog = -1;
  static constexpr Milliseconds kInstrumentationPeriod = 5000;
  if (lastInstrumentationLog < 0 ||
      currentTime - lastInstrumentationLog >= kInstrumentationPeriod) {
    char vTaskInfoStr[2048];
    vTaskGetRunTimeStats(vTaskInfoStr);
    vTaskInfoStr[sizeof(vTaskInfoStr) - 1] = '\0';
    info("%u INSTRUMENTATION:\n%s", currentTime, vTaskInfoStr);
    lastInstrumentationLog = currentTime;
  }
}

}  // namespace jazzlights

#endif  // JL_INSTRUMENTATION
