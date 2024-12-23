#include "jazzlights/fastled_runner.h"

#include "jazzlights/config.h"

#ifdef ESP32

#include "jazzlights/esp32_shared.h"
#include "jazzlights/instrumentation.h"
#include "jazzlights/ui/ui.h"
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
  SAVE_TIME_POINT(FastLed, Start);
  bool shouldWrite = false;
  uint8_t brightness;
#if JL_FASTLED_RUNNER_HAS_UI
  uint8_t uiBrightness;
  bool uiFresh;
#endif  // JL_FASTLED_RUNNER_HAS_UI
  {
    const std::lock_guard<std::mutex> lock(lockedLedMutex_);
    SAVE_TIME_POINT(FastLed, GetLock);
    brightness = brightnessLocked_;
#if JL_FASTLED_RUNNER_HAS_UI
    uiFresh = uiFreshLocked_;
    uiBrightness = uiBrightnessLocked_;
    if (uiFresh) {
      shouldWrite = true;
      memcpy(uiLedsFastLed_, uiLedsLocked_, uiLedMemorySize_);
      uiFreshLocked_ = false;
    }
#endif  // JL_FASTLED_RUNNER_HAS_UI
    for (size_t i = 0; i < renderers_.size(); i++) {
      if (renderers_[i]->copyLedsFromLockedToFastLed()) { shouldWrite = true; }
    }
  }
  SAVE_TIME_POINT(FastLed, Copy);
  SAVE_COUNT_POINT(LedPrintLoop);

  if (!shouldWrite) {
    // If there's nothing to write, the player is probably computing our next frame. Let's wait for it to notify us that
    // there's more data.
    (void)ulTaskGenericNotifyTake(kFastLedNotificationIndex, pdTRUE, portMAX_DELAY);
    SAVE_TIME_POINT(FastLed, WaitForNotify);
    // Return once the notification is called to restart this function from the top.
    return;
  }

  // This code initially would only write to a given renderer if there was data for that renderer. However, we later
  // realized that the FastLED code for WS2812B on ESP32 uses a shared RMT implementation that batches these writes to
  // send them in parallel. So we instead write to all renderers any time there's data to write to any renderer. This
  // better matches undocumented assumptions made inside the FastLED library, where they expect us to always write to
  // all strands. See <https://github.com/FastLED/FastLED/blob/master/src/platforms/esp/32/clockless_rmt_esp32.h>.

  ledWriteStart();
  SAVE_COUNT_POINT(LedPrintSend);
  for (size_t i = 0; i < renderers_.size(); i++) {
    uint8_t b = brightness;
#if JL_IS_CONFIG(STAFF)
    // Make the second strand of LEDs (the top piece of the staff) brighter.
    if (i > 0) { b += (255 - b) / 2; }
#endif  // STAFF
    renderers_[i]->sendToLeds(b);
  }
#if JL_FASTLED_RUNNER_HAS_UI
  uiController_->showLeds(uiBrightness);
#endif  // JL_FASTLED_RUNNER_HAS_UI
  numWritesThisEpoch_.fetch_add(1, std::memory_order_relaxed);
  ledWriteEnd();
  SAVE_TIME_POINT(FastLed, WriteToLeds);
}

void FastLedRunner::Render() {
  uint32_t brightness = player_->brightness();
  // Brightness may be reduced if this exceeds our power budget with the current pattern.

#if JL_MAX_MILLIWATTS > 0
  uint32_t powerAtFullBrightness = 0;
  for (auto& renderer : renderers_) { powerAtFullBrightness += renderer->GetPowerAtFullBrightness(); }
  const uint32_t powerAtDesiredBrightness =
      powerAtFullBrightness * brightness / 256;  // Forecast power at our current desired brightness.
  player_->set_power_limited(powerAtDesiredBrightness > JL_MAX_MILLIWATTS);
  if (player_->is_power_limited()) { brightness = brightness * JL_MAX_MILLIWATTS / powerAtDesiredBrightness; }

  jll_debug("pf%6" PRIu32 "    pd%5" PRIu32 "    bu%4u    bs%4" PRIu32 "    mW%5" PRIu32 "    mA%5" PRIu32 "%s",
            powerAtFullBrightness,
            powerAtDesiredBrightness,           // Full-brightness power, desired-brightness power.
            player_->brightness(), brightness,  // Desired and selected brightness.
            powerAtFullBrightness * brightness / 256,
            powerAtFullBrightness * brightness / 256 / 5,  // Selected power & current.
            player_->is_power_limited() ? " (limited)" : "");
#endif  // JL_MAX_MILLIWATTS

  SAVE_TIME_POINT(PrimaryRunLoop, Brightness);
  {
    const std::lock_guard<std::mutex> lock(lockedLedMutex_);
    SAVE_TIME_POINT(PrimaryRunLoop, GetLock);
    brightnessLocked_ = brightness;
    for (auto& renderer : renderers_) { renderer->copyLedsFromPlayerToLocked(); }
#if JL_FASTLED_RUNNER_HAS_UI
    uiFreshLocked_ = uiFreshPlayer_;
    if (uiFreshLocked_) {
      uiBrightnessLocked_ = uiBrightnessPlayer_;
      memcpy(uiLedsLocked_, uiLedsPlayer_, uiLedMemorySize_);
      uiFreshPlayer_ = false;
    }
#endif  // JL_FASTLED_RUNNER_HAS_UI
  }
  SAVE_TIME_POINT(PrimaryRunLoop, Copy);

  // Notify the FastLED task that there is new data to write.
  (void)xTaskGenericNotify(taskHandle_, kFastLedNotificationIndex,
                           /*notification_value=*/0, eNoAction, /*previousNotificationValue=*/nullptr);
  SAVE_TIME_POINT(PrimaryRunLoop, Notify);
  taskYIELD();
  SAVE_TIME_POINT(PrimaryRunLoop, Yield);
}

// static
void FastLedRunner::TaskFunction(void* parameters) {
  FastLedRunner* runner = reinterpret_cast<FastLedRunner*>(parameters);
#if JL_FASTLED_INIT_ON_OUR_TASK
  runner->Setup();
#endif  // JL_FASTLED_INIT_ON_OUR_TASK
  while (true) { runner->SendLedsToFastLed(); }
}

void FastLedRunner::Setup() {
  for (auto& renderer : renderers_) { renderer->Setup(); }
#if JL_FASTLED_RUNNER_HAS_UI
  SetupUi();
#endif  // JL_FASTLED_RUNNER_HAS_UI
  // Disable dithering, as we probably don't refresh quickly enough to benefit from it.
  // https://github.com/FastLED/FastLED/wiki/FastLED-Temporal-Dithering
  FastLED.setDither(DISABLE_DITHER);
  player_->SetNumLedWritesGetter(this);
}

void FastLedRunner::Start() {
#if !JL_FASTLED_INIT_ON_OUR_TASK
  Setup();
#endif  // !JL_FASTLED_INIT_ON_OUR_TASK
  // The primary runloop is pinned to core 1. We were initially planning to pin FastLED writes to core 0, but that
  // causes visual glitches due to various Bluetooth and/or Wi-Fi interrupts firing on that core while LEDs are being
  // written to, so we instead pin FastLED to core 1.
  BaseType_t ret = xTaskCreatePinnedToCore(TaskFunction, "FastLED", configMINIMAL_STACK_SIZE + 400,
                                           /*parameters=*/this, kHighestTaskPriority, &taskHandle_, /*coreID=*/1);
  if (ret != pdPASS) { jll_fatal("Failed to create FastLED task"); }
}

}  // namespace jazzlights

#endif  // ESP32
