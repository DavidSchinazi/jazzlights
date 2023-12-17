#include "jazzlights/fastled_runner.h"

#include "jazzlights/config.h"

#ifdef ARDUINO

#include "jazzlights/instrumentation.h"
#include "jazzlights/ui.h"
#include "jazzlights/util/log.h"

namespace jazzlights {

#if JL_IS_CONFIG(STAFF)
// Significantly reduce power limit since this is 24V and 6 LEDs per pixel.
#define JL_MAX_MILLIWATTS 300
#elif JL_IS_CONFIG(ROPELIGHT)
// Ropelight is generally connected to an independent large power source.
#define JL_MAX_MILLIWATTS 0
#elif JL_DEV
// Reduce power limit to 4W for host-connected development to avoid overloading USB port.
#define JL_MAX_MILLIWATTS 4200
#else
// Default power limit for USB-powered devices is 9W.
#define JL_MAX_MILLIWATTS 9200
// Note that these power levels are approximate. The FastLED power calculation algorithm is not entirely accurate, and
// probably not calibrated for the LED strips we use.
#endif

// Index 0 is normally reserved by FreeRTOS for stream and message buffers. However, the default precompiled FreeRTOS
// kernel for arduino/esp-idf only allows a single notification, so we use index 0 here. We don't use stream or message
// buffers on this specific task so we should be fine.
constexpr UBaseType_t kFastLedNotificationIndex = 0;
static_assert(kFastLedNotificationIndex < configTASK_NOTIFICATION_ARRAY_ENTRIES, "index too high");

void FastLedRunner::SendLedsToFastLed() {
  bool shouldWriteToAtLeastOne = false;
  std::vector<bool> shouldWrite(renderers_.size(), false);
  {
    const std::lock_guard<std::mutex> lock(lockedLedMutex_);
    for (size_t i = 0; i < renderers_.size(); i++) {
      shouldWrite[i] = renderers_[i]->copyLedsFromLockedToFastLed();
      if (shouldWrite[i]) { shouldWriteToAtLeastOne = true; }
    }
  }
  SAVE_COUNT_POINT(LedPrintLoop);
  if (!shouldWriteToAtLeastOne) { return; }

  uint32_t brightness = player_->GetBrightness();
  // Brightness may be reduced if this exceeds our power budget with the current pattern.

#if JL_MAX_MILLIWATTS > 0
  uint32_t powerAtFullBrightness = 0;
  for (auto& renderer : renderers_) { powerAtFullBrightness += renderer->GetPowerAtFullBrightness(); }
  const uint32_t powerAtDesiredBrightness =
      powerAtFullBrightness * brightness / 256;  // Forecast power at our current desired brightness.
  player_->SetPowerLimited(powerAtDesiredBrightness > JL_MAX_MILLIWATTS);
  if (player_->IsPowerLimited()) { brightness = brightness * JL_MAX_MILLIWATTS / powerAtDesiredBrightness; }

  jll_debug("pf%6u    pd%5u    bu%4u    bs%4u    mW%5u    mA%5u%s", powerAtFullBrightness,
            powerAtDesiredBrightness,              // Full-brightness power, desired-brightness power.
            player_->GetBrightness(), brightness,  // Desired and selected brightness.
            powerAtFullBrightness * brightness / 256,
            powerAtFullBrightness * brightness / 256 / 5,  // Selected power & current.
            player_->IsPowerLimited() ? " (limited)" : "");
#endif  // JL_MAX_MILLIWATTS
  SAVE_TIME_POINT(Brightness);

  ledWriteStart();
  SAVE_COUNT_POINT(LedPrintSend);
  for (size_t i = 0; i < renderers_.size(); i++) {
    uint32_t b = brightness;
#if JL_IS_CONFIG(STAFF)
    // Make the second strand of LEDs (the top piece of the staff) brighter.
    if (i > 0) { b += (255 - b) / 2; }
#endif  // STAFF
    if (shouldWrite[i]) { renderers_[i]->sendToLeds(b); }
  }
  SAVE_TIME_POINT(MainLED);
  ledWriteEnd();
}

void FastLedRunner::Render() {
  {
    const std::lock_guard<std::mutex> lock(lockedLedMutex_);
    for (auto& renderer : renderers_) { renderer->copyLedsFromPlayerToLocked(); }
  }

  // Notify the FastLED task that there is new data to write.
  (void)xTaskGenericNotify(taskHandle_, kFastLedNotificationIndex,
                           /*notification_value=*/0, eNoAction, /*previousNotificationValue=*/nullptr);
  vTaskDelay(1);  // Yield.
}

/*static*/ void FastLedRunner::TaskFunction(void* parameters) {
  FastLedRunner* runner = reinterpret_cast<FastLedRunner*>(parameters);
  while (true) {
    runner->SendLedsToFastLed();
    // Block this task until we are notified that there is new data to write.
    (void)ulTaskGenericNotifyTake(kFastLedNotificationIndex, pdTRUE, portMAX_DELAY);
  }
}

void FastLedRunner::Start() {
  // The Arduino loop is pinned to core 1 so we pin FastLED writes to core 0.
  BaseType_t ret = xTaskCreatePinnedToCore(TaskFunction, "FastLED", configMINIMAL_STACK_SIZE + 400,
                                           /*parameters=*/this,
                                           /*priority=*/30, &taskHandle_, /*coreID=*/0);
  if (ret != pdPASS) { jll_fatal("Failed to create FastLED task"); }
}

}  // namespace jazzlights

#endif  // ARDUINO
