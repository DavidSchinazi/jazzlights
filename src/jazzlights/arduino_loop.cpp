#include "jazzlights/arduino_loop.h"

#include "jazzlights/config.h"

#ifdef ARDUINO

#include <memory>
#include <mutex>

#include "jazzlights/fastled_runner.h"
#include "jazzlights/instrumentation.h"
#include "jazzlights/layout_data.h"
#include "jazzlights/networks/arduino_esp_wifi.h"
#include "jazzlights/networks/arduino_ethernet.h"
#include "jazzlights/networks/esp32_ble.h"
#include "jazzlights/player.h"
#include "jazzlights/ui.h"

#if JL_IS_CONTROLLER(CORE2AWS) || JL_IS_CONTROLLER(M5STAMP_PICO)
#define LED_PIN 32
#elif JL_IS_CONTROLLER(M5STAMP_C3U)
#define LED_PIN 1
#elif JL_IS_CONTROLLER(ATOM_MATRIX) || JL_IS_CONTROLLER(ATOM_LITE)
#define LED_PIN 26
#define LED_PIN2 32
#elif JL_IS_CONTROLLER(ATOM_S3)
#define LED_PIN 2
#define LED_PIN2 1
#else
#error "Unexpected controller"
#endif

namespace jazzlights {

#if JL_ARDUINO_ETHERNET
ArduinoEthernetNetwork ethernetNetwork(ArduinoEspWiFiNetwork::get()->getLocalDeviceId().PlusOne());
#endif  // JL_ARDUINO_ETHERNET
Player player;
FastLedRunner runner(&player);

void arduinoSetup(void) {
  Milliseconds currentTime = timeMillis();
  Serial.begin(115200);
  arduinoUiInitialSetup(player, currentTime);

#if JL_IS_CONFIG(STAFF)
  runner.AddLeds<WS2811, LED_PIN, RGB>(*GetLayout());
#elif JL_IS_CONFIG(ROPELIGHT)
  runner.AddLeds<WS2812, LED_PIN, BRG>(*GetLayout());
#else
  runner.AddLeds<WS2812B, LED_PIN, GRB>(*GetLayout());
#endif

#ifdef LED_PIN2
  if (GetLayout2()) { runner.AddLeds<WS2812B, LED_PIN2, GRB>(*GetLayout2()); }
#endif  // LED_PIN2

#if JL_IS_CONFIG(WAND) || JL_IS_CONFIG(STAFF) || JL_IS_CONFIG(CAPTAIN_HAT)
  player.setBasePrecedence(500);
  player.setPrecedenceGain(100);
#else
  player.setBasePrecedence(1000);
  player.setPrecedenceGain(1000);
#endif

  player.connect(Esp32BleNetwork::get());
  player.connect(ArduinoEspWiFiNetwork::get());
#if JL_ARDUINO_ETHERNET
  player.connect(&ethernetNetwork);
#endif  // JL_ARDUINO_ETHERNET
  player.begin(currentTime);

  arduinoUiFinalSetup(player, currentTime);

  runner.Start();
}

void arduinoLoop(void) {
  SAVE_TIME_POINT(LoopStart);
  Milliseconds currentTime = timeMillis();
  arduinoUiLoop(player, currentTime);
  SAVE_TIME_POINT(UserInterface);
  Esp32BleNetwork::get()->runLoop(currentTime);
  SAVE_TIME_POINT(Bluetooth);

  const bool shouldRender = player.render(currentTime);
  SAVE_TIME_POINT(Player);
  if (shouldRender) { runner.Render(); }
}

}  // namespace jazzlights

#endif  // ARDUINO
