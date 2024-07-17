#include "jazzlights/config.h"

#ifndef PIO_UNIT_TESTING

#ifdef ESP32

#define JL_ARDUINO_LOOP_ON_OTHER_CORE 0
// While we have confirmed that moving the arduino loop to core 0 works, it actually performs slightly worse as there is
// more contention on that core because that's where a lot of other things (Wi-Fi, Bluetooth, timers) run. Even though
// we're writing to the LEDs on core 1, that takes up very little CPU time because most of the work is performed by the
// RMT module when writing to WS2812B. In practice both options allow reaching 100 computed FPS and 89 written FPS on
// the vests, which is currently our highest LED count. So we have it disabled for now, but could enable it for other
// devices with different properties.

#include "jazzlights/arduino_loop.h"

#if JL_ARDUINO_LOOP_ON_OTHER_CORE

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "jazzlights/util/log.h"

namespace jazzlights {
namespace {

TaskHandle_t gTaskHandle = nullptr;

void ArduinoTaskFunction(void* parameters) {
  arduinoSetup();
  while (true) {
    arduinoLoop();
    // Without this delay, the watchdog fires because the IDLE task never gets to run. Another option would be to run
    // this task at tskIDLE_PRIORITY, but in practice that performs poorly and we get FPS way below 100.
    vTaskDelay(2);
  }
}

void SetupArduinoTask() {
  BaseType_t ret = xTaskCreatePinnedToCore(ArduinoTaskFunction, "ArduinoJL", CONFIG_ARDUINO_LOOP_STACK_SIZE,
                                           /*parameters=*/nullptr,
                                           /*priority=*/30, &gTaskHandle, /*coreID=*/0);
  if (ret != pdPASS) { jll_fatal("Failed to create ArduinoJL task"); }
}

}  // namespace
}  // namespace jazzlights

#endif  // JL_ARDUINO_LOOP_ON_OTHER_CORE

void setup() {
#if JL_ARDUINO_LOOP_ON_OTHER_CORE
  jazzlights::SetupArduinoTask();
#else   // JL_ARDUINO_LOOP_ON_OTHER_CORE
  jazzlights::arduinoSetup();
#endif  // JL_ARDUINO_LOOP_ON_OTHER_CORE
}

void loop() {
#if JL_ARDUINO_LOOP_ON_OTHER_CORE
  vTaskDelay(1000000 / portTICK_PERIOD_MS);
#else   // JL_ARDUINO_LOOP_ON_OTHER_CORE
  jazzlights::arduinoLoop();
#endif  // JL_ARDUINO_LOOP_ON_OTHER_CORE
}

#else  // ESP32

int main(int /*argc*/, char** /*argv*/) { return 0; }

#endif  // ESP32

#endif  // PIO_UNIT_TESTING
