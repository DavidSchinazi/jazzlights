#include <Jazzlights.h>
#include "jazzlights/instrumentation.h"

#if WEARABLE

#if CORE2AWS
#  define LED_PIN  32
#elif defined(ESP32)
#  define LED_PIN  26
#  define LED_PIN2 32
#elif defined(ESP8266)
#  define LED_PIN  5
#else
#  error "Unexpected board"
#endif

namespace jazzlights {

CRGB leds[LEDNUM] = {};

void renderPixel(int i, uint8_t r, uint8_t g, uint8_t b) {
  leds[i] = CRGB(r, g, b);
}

#if LEDNUM2
CRGB leds2[LEDNUM2] = {};

void renderPixel2(int i, uint8_t r, uint8_t g, uint8_t b) {
  leds2[i] = CRGB(r, g, b);
}
#endif  // LEDNUM2

EspWiFi network("FISHLIGHT", "155155155");
#if JAZZLIGHTS_ARDUINO_ETHERNET
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
#endif  // JAZZLIGHTS_ARDUINO_ETHERNET
Player player;

CLEDController* mainVestController = nullptr;
#if LEDNUM2
CLEDController* mainVestController2 = nullptr;
#endif  // LEDNUM2

void vestSetup(void) {
  Milliseconds currentTime = timeMillis();
  Serial.begin(115200);
#if CORE2AWS
  core2SetupStart(player, currentTime);
#else  // CORE2AWS
  setupButtons();
#endif  // CORE2AWS
  player.addStrand(*GetLayout(), renderPixel);
#if LEDNUM2
  player.addStrand(*GetLayout2(), renderPixel2);
#endif  // LEDNUM2
#if GECKO_FOOT
  player.setBasePrecedence(2500);
  player.setPrecedenceGain(1000);
#elif FAIRY_WAND || IS_STAFF || IS_CAPTAIN_HAT
  player.setBasePrecedence(500);
  player.setPrecedenceGain(100);
#else
  player.setBasePrecedence(1000);
  player.setPrecedenceGain(1000);
#endif

#if ESP32_BLE
  player.connect(Esp32BleNetwork::get());
#endif // ESP32_BLE
  player.connect(&network);
#if JAZZLIGHTS_ARDUINO_ETHERNET
  player.connect(&ethernetNetwork);
#endif  // JAZZLIGHTS_ARDUINO_ETHERNET
  player.begin(currentTime);

#if GECKO_SCALES
  // Note to self for future reference: we were able to get the 2018 Gecko Robot scales to light
  // up correctly with the M5Stack ATOM Matrix without any level shifters.
  // Wiring of the 2018 Gecko Robot scales: (1) = Data, (2) = Ground, (3) = Clock, (4) = 12VDC
  // if we number the wires on the male connector (assuming notch is up top):
  // (1)  (4)
  // (2)  (3)
  // conversely on the female connector (also assuming notch is up top):
  // (4)  (1)
  // (3)  (2)
  // M5 black wire = Ground = scale (2) = scale pigtail black (also connect 12VDC power supply ground here)
  // M5 red wire = 5VDC power supply - not connected to scale pigtail
  // M5 yellow wire = G26 = Data = scale (1) = scale pigtail yellow/green
  // M5 white wire = G32 = Clock = scale (3) = scale pigtail blue in some cases, brown in others
  // 12VDC power supply = scale (4) = scale pigtail brown in some cases, blue in others
  // IMPORTANT: it appears that on some pigtails brown and blue are inverted.
  // Separately, on the two-wire pigtails for power injection, blue is 12VDC and brown is Ground.
  // IMPORTANT: the two-wire pigtail is unfortunately reversible, and needs to be plugged in such
  // that the YL inscription is on the male end of the three-way power-injection splitter.
  mainVestController = &FastLED.addLeds</*CHIPSET=*/WS2801, /*DATA_PIN=*/26, /*CLOCK_PIN=*/32, /*RGB_ORDER=*/GBR>(
    leds, sizeof(leds)/sizeof(*leds));
#elif IS_STAFF
  mainVestController = &FastLED.addLeds<WS2811, LED_PIN, RGB>(
    leds, sizeof(leds)/sizeof(*leds));
#elif IS_ROPELIGHT
  mainVestController = &FastLED.addLeds<WS2811, LED_PIN, BRG>(
    leds, sizeof(leds)/sizeof(*leds));
#else  // Vest.
  mainVestController = &FastLED.addLeds<WS2812B, LED_PIN, GRB>(
    leds, sizeof(leds)/sizeof(*leds));
#endif
#if LEDNUM2
  mainVestController2 = &FastLED.addLeds<WS2812B, LED_PIN2, GRB>(
    leds2, sizeof(leds2)/sizeof(*leds2));
#endif  // LEDNUM2
#if CORE2AWS
  core2SetupEnd(player, currentTime);
#endif  // CORE2AWS
}

void vestLoop(void) {
  SAVE_TIME_POINT(LoopStart);
  Milliseconds currentTime = timeMillis();
#if CORE2AWS
  core2Loop(player, currentTime);
#else // CORE2AWS
  SAVE_TIME_POINT(Core2);
  // Read, debounce, and process the buttons, and perform actions based on button state.
  doButtons(player,
            network,
#if ESP32_BLE
            *Esp32BleNetwork::get(),
#endif // ESP32_BLE
            currentTime);
#endif // CORE2AWS
  SAVE_TIME_POINT(Buttons);

#if ESP32_BLE
  Esp32BleNetwork::get()->runLoop(currentTime);
#endif // ESP32_BLE
  SAVE_TIME_POINT(Bluetooth);

  const bool shouldRender = player.render(currentTime);
  SAVE_TIME_POINT(Player);
  if (!shouldRender) {
    return;
  }
  uint32_t brightness = getBrightness();        // May be reduced if this exceeds our power budget with the current pattern

#if MAX_MILLIWATTS
  uint32_t powerAtFullBrightness = calculate_unscaled_power_mW(mainVestController->leds(), mainVestController->size());
#if LEDNUM2
  powerAtFullBrightness += calculate_unscaled_power_mW(mainVestController2->leds(), mainVestController2->size());
#endif // LEDNUM2
  const uint32_t powerAtDesiredBrightness = powerAtFullBrightness * brightness / 256;         // Forecast power at our current desired brightness
  player.powerLimited = (powerAtDesiredBrightness > MAX_MILLIWATTS);
  if (player.powerLimited) { brightness = brightness * MAX_MILLIWATTS / powerAtDesiredBrightness; }

  debug("pf%6u    pd%5u    bu%4u    bs%4u    mW%5u    mA%5u%s",
    powerAtFullBrightness, powerAtDesiredBrightness,                                          // Full-brightness power, desired-brightness power
    getBrightness(), brightness,                                                              // Desired and selected brightness
    powerAtFullBrightness * brightness / 256, powerAtFullBrightness * brightness / 256 / 5,   // Selected power & current
    player.powerLimited ? " (limited)" : "");
#endif  // MAX_MILLIWATTS
  SAVE_TIME_POINT(Brightness);

  mainVestController->showLeds(brightness);
  SAVE_TIME_POINT(MainLED);
#if LEDNUM2
  mainVestController2->showLeds(brightness);
#endif  // LEDNUM2
  SAVE_TIME_POINT(SecondLED);
}

std::string wifiStatus(Milliseconds currentTime) {
  return network.statusStr(currentTime);
}

std::string bleStatus(Milliseconds currentTime) {
#if ESP32_BLE
  return Esp32BleNetwork::get()->statusStr(currentTime);
#else  // ESP32_BLE
  return "Not supported";
#endif  // ESP32_BLE
}

std::string otherStatus(Player& player, Milliseconds currentTime) {
  char otherStatusStr[100] = "Leading";
  if (player.followedNextHopNetwork() == &network) {
    snprintf(otherStatusStr, sizeof(otherStatusStr) - 1, "Following Wi-Fi nh=%u", player.currentNumHops());
  }
#if ESP32_BLE
  else if (player.followedNextHopNetwork() == Esp32BleNetwork::get()) {
    snprintf(otherStatusStr, sizeof(otherStatusStr) - 1, "Following BLE nh=%u", player.currentNumHops());
  }
#endif  // ESP32_BLE
  return std::string(otherStatusStr);
}

}  // namespace jazzlights

#endif // WEARABLE
