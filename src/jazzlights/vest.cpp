#include "jazzlights/vest.h"

#include "jazzlights/config.h"

#ifdef ARDUINO

#include <memory>
#include <mutex>

#include "jazzlights/board.h"
#include "jazzlights/button.h"
#include "jazzlights/core2.h"
#include "jazzlights/fastled_runner.h"
#include "jazzlights/instrumentation.h"
#include "jazzlights/networks/arduino_esp_wifi.h"
#include "jazzlights/networks/arduino_ethernet.h"
#include "jazzlights/networks/esp32_ble.h"
#include "jazzlights/player.h"

#if CORE2AWS
#define LED_PIN 32
#elif defined(ESP32)
#if M5STAMP_PICO
#define LED_PIN 32
#elif M5STAMP_C3U
#define LED_PIN 1
#else  // M5STAMP
#define LED_PIN 26
#define LED_PIN2 32
#endif  // M5STAMP
#else
#error "Unexpected board"
#endif

#ifndef BRIGHTER2
#define BRIGHTER2 0
#endif  // BRIGHTER2

namespace jazzlights {

#if JAZZLIGHTS_ARDUINO_ETHERNET
ArduinoEthernetNetwork ethernetNetwork(ArduinoEspWiFiNetwork::get()->getLocalDeviceId().PlusOne());
#endif  // JAZZLIGHTS_ARDUINO_ETHERNET
Player player;
FastLedRunner runner(&player);

void vestSetup(void) {
  Milliseconds currentTime = timeMillis();
  Serial.begin(115200);
#if CORE2AWS
  core2SetupStart(player, currentTime);
#else   // CORE2AWS
  setupButtons();
#endif  // CORE2AWS

#if IS_STAFF
  runner.AddLeds<WS2811, LED_PIN, RGB>(*GetLayout());
#elif IS_ROPELIGHT
  runner.AddLeds<WS2812, LED_PIN, BRG>(*GetLayout());
#else  // Vest.
  runner.AddLeds<WS2812B, LED_PIN, GRB>(*GetLayout());
#endif

#ifdef LED_PIN2
  if (GetLayout2()) { runner.AddLeds<WS2812B, LED_PIN2, GRB>(*GetLayout2()); }
#endif  // LED_PIN2

#if FAIRY_WAND || IS_STAFF || IS_CAPTAIN_HAT
  player.setBasePrecedence(500);
  player.setPrecedenceGain(100);
#else
  player.setBasePrecedence(1000);
  player.setPrecedenceGain(1000);
#endif

#if ESP32_BLE
  player.connect(Esp32BleNetwork::get());
#endif  // ESP32_BLE
  player.connect(ArduinoEspWiFiNetwork::get());
#if JAZZLIGHTS_ARDUINO_ETHERNET
  player.connect(&ethernetNetwork);
#endif  // JAZZLIGHTS_ARDUINO_ETHERNET
  player.begin(currentTime);

#if CORE2AWS
  core2SetupEnd(player, currentTime);
#endif  // CORE2AWS

  runner.Start();
}

void vestLoop(void) {
  SAVE_TIME_POINT(LoopStart);
  Milliseconds currentTime = timeMillis();
#if CORE2AWS
  core2Loop(player, currentTime);
#else  // CORE2AWS
  SAVE_TIME_POINT(Core2);
  // Read, debounce, and process the buttons, and perform actions based on button state.
  doButtons(player, *ArduinoEspWiFiNetwork::get(),
#if ESP32_BLE
            *Esp32BleNetwork::get(),
#endif  // ESP32_BLE
            currentTime);
#endif  // CORE2AWS
  SAVE_TIME_POINT(Buttons);

#if ESP32_BLE
  Esp32BleNetwork::get()->runLoop(currentTime);
#endif  // ESP32_BLE
  SAVE_TIME_POINT(Bluetooth);

  const bool shouldRender = player.render(currentTime);
  SAVE_TIME_POINT(Player);
  if (shouldRender) { runner.Render(); }
}

}  // namespace jazzlights

#endif  // ARDUINO
