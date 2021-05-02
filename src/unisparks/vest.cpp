#include <Unisparks.h>

#if !defined(ESP8266) && !defined(ESP32)
#  error "Unexpected board"
#endif

namespace unisparks {

CRGB leds[MATRIX_WIDTH*MATRIX_HEIGHT] = {};

void renderPixel(int i, uint8_t r, uint8_t g, uint8_t b) {
  leds[i] = CRGB(r, g, b);
}

Esp8266WiFi network("FISHLIGHT", "155155155");
Player player;
Matrix pixels(LEDNUM, 1);

CLEDController* mainVestController = nullptr;

void vestSetup(void) {
  Serial.begin(115200);

  setupButtons();

  player.begin(pixels, renderPixel, network);

  mainVestController = &FastLED.addLeds<WS2812B, LED_PIN, GRB>(
    leds, sizeof(leds)/sizeof(*leds));

  pushBrightness();
}

void vestLoop(void) {
  uint32_t currentMillis = (uint32_t)millis();
  updateButtons(currentMillis); // read, debounce, and process the buttons
  doButtons(player, currentMillis); // perform actions based on button state
  player.render();
  mainVestController->showLeds(getBrightness());
}

} // namespace unisparks
