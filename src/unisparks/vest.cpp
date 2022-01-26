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
  uint32_t br = getBrightness();        // May be reduced if this exceeds our power budget with the current pattern

#if MAX_MW
  const uint32_t pf = calculate_unscaled_power_mW(mainVestController->leds(), mainVestController->size());
  const uint32_t pd = pf * br / 256;    // Forcast power at our current desired brightness
  player.powerlimited = (pd > MAX_MW);
  if (player.powerlimited) br = br * MAX_MW / pd;

  debug("pf%6u    pd%5u    bu%4u    bs%4u    mW%5u    mA%5u%s",
    pf, pd,                             // Full-brightness power, desired-brightness power
    getBrightness(), br,                // Desired and selected brightness
    pf * br / 256, pf * br / 256 / 5,   // Selected power & current
    player.powerlimited ? " (limited)" : "");
#endif

  mainVestController->showLeds(br);
}

} // namespace unisparks

#endif // WEARABLE
