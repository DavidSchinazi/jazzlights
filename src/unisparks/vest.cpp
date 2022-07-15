#include <Unisparks.h>

#if WEARABLE

#if defined(ESP32)
#  define LED_PIN  26
#elif defined(ESP8266)
#  define LED_PIN  5
#else
#  error "Unexpected board"
#endif

namespace unisparks {

CRGB leds[LEDNUM] = {};

void renderPixel(int i, uint8_t r, uint8_t g, uint8_t b) {
  leds[i] = CRGB(r, g, b);
}

Esp8266WiFi network("FISHLIGHT", "155155155");
#if UNISPARKS_ARDUINO_ETHERNET
NetworkDeviceId GetEthernetDeviceId() {
  NetworkDeviceId deviceId = network.getLocalDeviceId();
  deviceId.data()[5]++;
  if (deviceId.data()[5] == 0) {
    deviceId.data()[4]++;
    if (deviceId.data()[4] == 0) {
      deviceId.data()[3]++;
      if (deviceId.data()[3] == 0) {
        deviceId.data()[2]++;
        if (deviceId.data()[2] == 0) {
          deviceId.data()[1]++;
          if (deviceId.data()[1] == 0) {
            deviceId.data()[0]++;
          }
        }
      }
    }
  }
  return deviceId;
}
ArduinoEthernetNetwork ethernetNetwork(GetEthernetDeviceId());
#endif  // UNISPARKS_ARDUINO_ETHERNET
Player player;

CLEDController* mainVestController = nullptr;

void vestSetup(void) {
  Serial.begin(115200);

  setupButtons();
  player.addStrand(*GetLayout(), renderPixel);
  player.setBasePrecedence(1000);
  player.setPrecedenceGain(1000);

#if ESP32_BLE
  player.connect(Esp32BleNetwork::get());
#endif // ESP32_BLE
  player.connect(&network);
#if UNISPARKS_ARDUINO_ETHERNET
  player.connect(&ethernetNetwork);
#endif  // UNISPARKS_ARDUINO_ETHERNET
  player.begin(timeMillis());

  // Note to self for future reference: we were able to get the 2018 Gecko Robot scales to light
  // up correctly with the M5Stack ATOM Matrix without any level shifters by connecting:
  // M5 black wire - ground - scale pigtail black (also connect 12VDC power supply ground here)
  // M5 red wire - 5VDC power supply - not connected to scale pigtail
  // M5 yellow wire - G26 - data - scale pigtail yellow
  // M5 white wire - G32 - clock - scale pigtail blue
  // 12VDC power supply - scale pigtail brown
  // FastLED.addLeds</*CHIPSET=*/WS2801, /*DATA_PIN=*/26, /*CLOCK_PIN=*/32, /*RGB_ORDER=*/GBR>
  // TODO refactor this comment into a PlatformIO environment.

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
