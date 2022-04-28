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
  // Make sure BLE is properly setup by constructor.
  (void)Esp32BleNetwork::get();
#endif // ESP32_BLE

  setupButtons();

  player.begin(pixels, renderPixel, network);

  mainVestController = &FastLED.addLeds<WS2812B, LED_PIN, GRB>(
    leds, sizeof(leds)/sizeof(*leds));
}

void vestLoop(void) {
  Milliseconds currentTime = timeMillis();
  doButtons(player, network.status(), currentTime); // Read, debounce, and process the buttons, and perform actions based on button state

#if ESP32_BLE
  Esp32BleNetwork::get()->runLoop(currentTime);
#endif // ESP32_BLE

  player.render(currentTime);
  uint32_t brightness = getBrightness();        // May be reduced if this exceeds our power budget with the current pattern

#if MAX_MILLIWATTS
  const uint32_t powerAtFullBrightness    = calculate_unscaled_power_mW(mainVestController->leds(), mainVestController->size());
  const uint32_t powerAtDesiredBrightness = powerAtFullBrightness * brightness / 256;         // Forecast power at our current desired brightness
  player.powerLimited = (powerAtDesiredBrightness > MAX_MILLIWATTS);
  if (player.powerLimited) { brightness = brightness * MAX_MILLIWATTS / powerAtDesiredBrightness; }

  debug("pf%6u    pd%5u    bu%4u    bs%4u    mW%5u    mA%5u%s",
    powerAtFullBrightness, powerAtDesiredBrightness,                                          // Full-brightness power, desired-brightness power
    getBrightness(), brightness,                                                              // Desired and selected brightness
    powerAtFullBrightness * brightness / 256, powerAtFullBrightness * brightness / 256 / 5,   // Selected power & current
    player.powerLimited ? " (limited)" : "");
#endif

  mainVestController->showLeds(brightness);
}

} // namespace unisparks

#endif // WEARABLE
