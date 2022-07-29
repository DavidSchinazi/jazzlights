#include <Unisparks.h>

#if WEARABLE

#if CORE2AWS
#  include <M5Core2.h>
#  define LED_PIN  32
#elif defined(ESP32)
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

#if CORE2AWS

Matrix core2ScreenPixels(40, 30);

class Core2ScreenRenderer : public Renderer {
 public:
  Core2ScreenRenderer() {}
  void toggleEnabled() {
    enabled_ = !enabled_;
    if (enabled_) {
      M5.Lcd.startWrite();
      M5.Lcd.writecommand(ILI9341_DISPON);
      M5.Lcd.endWrite();
      M5.Lcd.wakeup();
      M5.Lcd.setBrightness(80);
      M5.Buttons.draw();
    } else {
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setBrightness(0);
      M5.Lcd.sleep();
      M5.Lcd.startWrite();
      M5.Lcd.writecommand(ILI9341_DISPOFF);
      M5.Lcd.endWrite();
    }
  }
  void render(InputStream<Color>& pixelColors) override {
    if (!enabled_) { return; }
    size_t i = 0;
    uint16_t rowColors16[320 * 8];
    for (Color color : pixelColors) {
      RgbColor rgb = color.asRgb();
      uint16_t color16 = ((uint16_t)(rgb.red & 0xF8) << 8) |
                         ((uint16_t)(rgb.green & 0xFC) << 3) | \
                         ((rgb.blue & 0xF8) >> 3);
      int32_t x = i % 40;
      int32_t y = i / 40;
      int32_t factor = fullScreen_ ? 8 : 4;
      for (size_t xi = 0; xi < factor; xi++) {
        for (size_t yi = 0; yi < factor; yi++) {
          rowColors16[x * factor + xi + yi * 40 * factor] = color16;
        }
      }
      if (x == 39) {
        bool swap = M5.Lcd.getSwapBytes();
        M5.Lcd.setSwapBytes(true);
        M5.Lcd.pushImage(/*x0=*/0, /*y0=*/y * factor, /*w=*/40 * factor, /*h=*/factor, rowColors16);
        M5.Lcd.setSwapBytes(swap);
      }
      i++;
    }
  }
 private:
  bool enabled_ = true;
  bool fullScreen_ = false;
};

Core2ScreenRenderer core2ScreenRenderer;

ButtonColors onCol = {BLACK, WHITE, WHITE};
ButtonColors offCol = {RED, WHITE, WHITE};
Button nextButton(/*x=*/165, /*y=*/5, /*w=*/150, /*h=*/50, /*rot1=*/false, "Next", onCol, offCol);
Button loopButton(/*x=*/165, /*y=*/65, /*w=*/150, /*h=*/50, /*rot1=*/false, "Loop", onCol, offCol);
Button patternControlButton(/*x=*/5, /*y=*/125, /*w=*/150, /*h=*/110, /*rot1=*/false, "Pattern Control", onCol, offCol);

#endif  // CORE2AWS

void vestSetup(void) {
  Serial.begin(115200);
#if CORE2AWS
  M5.begin();
  M5.Lcd.fillScreen(BLACK);
  M5.Buttons.draw();
  player.addStrand(core2ScreenPixels, core2ScreenRenderer);
#else  // CORE2AWS
  setupButtons();
#endif  // CORE2AWS
  player.addStrand(*GetLayout(), renderPixel);
#if GECKO_FOOT
  player.setBasePrecedence(2500);
  player.setPrecedenceGain(1000);
#elif FAIRY_WAND
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
#if UNISPARKS_ARDUINO_ETHERNET
  player.connect(&ethernetNetwork);
#endif  // UNISPARKS_ARDUINO_ETHERNET
  player.begin(timeMillis());

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
#elif STAFF
  mainVestController = &FastLED.addLeds<WS2811, LED_PIN, RGB>(
    leds, sizeof(leds)/sizeof(*leds));
#else  // Vest.
  mainVestController = &FastLED.addLeds<WS2812B, LED_PIN, GRB>(
    leds, sizeof(leds)/sizeof(*leds));
#endif
}

void vestLoop(void) {
  Milliseconds currentTime = timeMillis();
#if CORE2AWS
  M5.Touch.update();
  M5.Buttons.update();
  if (M5.background.wasPressed()) {
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t totalHeap = ESP.getHeapSize();
    uint32_t freePSRAM = ESP.getFreePsram();
    uint32_t totalPSRAM = ESP.getPsramSize();
    info("%u background pressed, free RAM %u/%u free PSRAM %u/%u",
         currentTime, freeHeap, totalHeap, freePSRAM, totalPSRAM);
    core2ScreenRenderer.toggleEnabled();
  }
  if (nextButton.wasPressed()) {
    info("%u next pressed", currentTime);
    loopButton.setLabel("Loop");
    loopButton.draw();
    player.next(currentTime);
  }
  if (loopButton.wasPressed()) {
    info("%u loop pressed", currentTime);
    loopButton.setLabel("Looping");
    loopButton.draw();
    player.loopOne(currentTime);
  }
#else // CORE2AWS
  // Read, debounce, and process the buttons, and perform actions based on button state.
  doButtons(player,
            network,
#if ESP32_BLE
            *Esp32BleNetwork::get(),
#endif // ESP32_BLE
            currentTime);
#endif // CORE2AWS

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
