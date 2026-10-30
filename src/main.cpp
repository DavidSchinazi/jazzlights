#include "jazzlights/util/config.h"

#ifndef PIO_UNIT_TESTING

#ifdef ESP32

#define JL_PRIMARY_RUNLOOP_ON_OTHER_CORE 0
// While we have confirmed that moving the primary runloop to core 0 works, it actually performs slightly worse as
// there is more contention on that core because that's where a lot of other things (Wi-Fi, Bluetooth, timers) run.
// Even though we're writing to the LEDs on core 1, that takes up very little CPU time because most of the work is
// performed by the RMT module when writing to WS2812B. In practice both options allow reaching 100 computed FPS and
// 89 written FPS on the vests, which is currently our highest LED count. So we have it disabled for now, but could
// enable it for other devices with different properties.

#include "jazzlights/primary_runloop.h"
#include "jazzlights/util/esp32_shared.h"

#if JL_PRIMARY_RUNLOOP_ON_OTHER_CORE

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "jazzlights/util/log.h"

namespace jazzlights {
namespace {

TaskHandle_t gTaskHandle = nullptr;

void PrimaryRunLoopTaskFunction(void* parameters) {
  SetupPrimaryRunLoop();
  while (true) {
    RunPrimaryRunLoop();
    // Without this delay, the watchdog fires because the IDLE task never gets to run. Another option would be to run
    // this task at tskIDLE_PRIORITY, but in practice that performs poorly and we get FPS way below 100.
    vTaskDelay(2);
  }
}

void SetupPrimaryRunLoopTask() {
  BaseType_t ret = xTaskCreatePinnedToCore(PrimaryRunLoopTaskFunction, "PrimaryRunLoopJL", kPrimaryRunLoopStackSize,
                                           /*parameters=*/nullptr, kHighestTaskPriority, &gTaskHandle, /*coreID=*/0);
  if (ret != pdPASS) { jll_fatal("Failed to create PrimaryRunLoopJL task"); }
}

}  // namespace
}  // namespace jazzlights

#endif  // JL_PRIMARY_RUNLOOP_ON_OTHER_CORE

void setup() {
#if JL_PRIMARY_RUNLOOP_ON_OTHER_CORE
  jazzlights::SetupPrimaryRunLoopTask();
#else   // JL_PRIMARY_RUNLOOP_ON_OTHER_CORE
  jazzlights::SetupPrimaryRunLoop();
#endif  // JL_PRIMARY_RUNLOOP_ON_OTHER_CORE
}

void loop() {
#if JL_PRIMARY_RUNLOOP_ON_OTHER_CORE
  vTaskDelay(1000000 / portTICK_PERIOD_MS);
#else   // JL_PRIMARY_RUNLOOP_ON_OTHER_CORE
  jazzlights::RunPrimaryRunLoop();
#endif  // JL_PRIMARY_RUNLOOP_ON_OTHER_CORE
}

#if JL_ARDUINO

#include <chip-debug-report.h>
#include <esp32-hal-log.h>

#if ARDUHAL_LOG_LEVEL < ARDUHAL_LOG_LEVEL_DEBUG
// Below the debug log level, the Arduino core only prints its chip debug report if shouldPrintChipDebugReport() is
// overridden to return true, which we never do. Providing empty implementations here prevents the linker from pulling
// in the report code, which saves about 4KB of flash.
void printBeforeSetupInfo() {}
void printAfterSetupInfo() {}
#endif  // ARDUHAL_LOG_LEVEL < ARDUHAL_LOG_LEVEL_DEBUG

#else  // JL_ARDUINO

#include <esp_ota_ops.h>
#include <nvs_flash.h>

#include "jazzlights/util/log.h"

// The Arduino core switches the CPU to the board's f_cpu at startup. Without it, the CPU frequency comes from
// sdkconfig, see sdkconfig.idf.defaults.<target>. F_CPU is defined by extras/scripts/fastled_idf_shim.py.
static_assert(F_CPU == CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1000000, "sdkconfig CPU frequency does not match board f_cpu");

namespace jazzlights {
namespace {

// Replaces the Arduino core's loopTask, which calls setup() once and then loop() forever.
void LoopTaskFunction(void* /*parameters*/) {
  setup();
  while (true) { loop(); }
}

// Performs the parts of the Arduino core's app_main() and initArduino() that we rely on.
void StartLoopTask() {
#if CONFIG_APP_ROLLBACK_ENABLE
  // Confirm that this image works, otherwise the bootloader would roll back to the previous one after an OTA update.
  esp_ota_img_states_t otaState;
  if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &otaState) == ESP_OK &&
      otaState == ESP_OTA_IMG_PENDING_VERIFY) {
    ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
  }
#endif  // CONFIG_APP_ROLLBACK_ENABLE
  // NVS is required by the Bluetooth and Wi-Fi stacks to store PHY calibration data.
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  BaseType_t ret =
      xTaskCreatePinnedToCore(LoopTaskFunction, "loopTask", kPrimaryRunLoopStackSize,
                              /*parameters=*/nullptr, /*priority=*/1, /*taskHandle=*/nullptr, kPrimaryRunLoopCore);
  if (ret != pdPASS) { jll_fatal("Failed to create loopTask"); }
}

}  // namespace
}  // namespace jazzlights

extern "C" void app_main() { jazzlights::StartLoopTask(); }

#endif  // JL_ARDUINO

#else  // ESP32

int main(int /*argc*/, char** /*argv*/) { return 0; }

#endif  // ESP32

#endif  // PIO_UNIT_TESTING
