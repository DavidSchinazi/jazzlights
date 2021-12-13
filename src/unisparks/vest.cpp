#include <Unisparks.h>

#if WEARABLE

#if !defined(ESP8266) && !defined(ESP32)
#  error "Unexpected board"
#endif

namespace unisparks {

CRGB leds[LEDNUM] = {};

void renderPixel(int i, uint8_t r, uint8_t g, uint8_t b) {
  leds[i] = CRGB(r, g, b);
}

Esp8266WiFi network("FISHLIGHT", "155155155");
Player player;
ReverseMap<LEDNUM> pixels(pixelMap, MATRIX_WIDTH, MATRIX_HEIGHT);

CLEDController* mainVestController = nullptr;

void vestSetup(void) {
  Serial.begin(115200);

#if ESP32_BLE
  Esp32Ble::Setup();
#endif // ESP32_BLE

  setupButtons();

  player.begin(pixels, renderPixel, network);

  mainVestController = &FastLED.addLeds<WS2812B, LED_PIN, GRB>(
    leds, sizeof(leds)/sizeof(*leds));

  pushBrightness();
}

void vestLoop(void) {
  Milliseconds currentTime = timeMillis();
  uint32_t currentMillis = (uint32_t)currentTime;
  updateButtons(currentMillis); // read, debounce, and process the buttons
  doButtons(player, currentMillis); // perform actions based on button state

#if ESP32_BLE
  Esp32Ble::Loop(currentTime);
#endif // ESP32_BLE

  player.render();
  mainVestController->showLeds(getBrightness());
}

} // namespace unisparks

#endif // WEARABLE
