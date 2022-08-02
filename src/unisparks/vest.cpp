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

void setCore2ScreenBrightness(uint8_t brightness, bool allowUnsafe = false) {
  // brightness of 0 means backlight off.
  // max safe brightness is 20, max unsafe brightness is 36.
  // levels between 21 and 36 included should be used for short periods of time only.
  // Inspired by https://github.com/Sarah-C/M5Stack_Core2_ScreenBrightness
  if (brightness == 0) {
    M5.Axp.SetDCDC3(false);
    return;
  }
  M5.Axp.SetDCDC3(true);
  const uint8_t maxBrightness = allowUnsafe ? 36 : 20;
  if (brightness > maxBrightness) { brightness = maxBrightness; }
  const uint16_t backlightVoltage = 2300 + brightness * 25;
  constexpr uint8_t kBacklightVoltageAddress = 2;
  M5.Axp.SetDCVoltage(kBacklightVoltageAddress, backlightVoltage);
}

enum class ScreenMode {
  kMainMenu,
  kFullScreenPattern,
  kPatternControlMenu,
  kSystemMenu,
};
ScreenMode gScreenMode = ScreenMode::kMainMenu;

Matrix core2ScreenPixels(40, 30);

class Core2ScreenRenderer : public Renderer {
 public:
  Core2ScreenRenderer() {}
  void setFullScreen(bool fullScreen) { fullScreen_ = fullScreen; }
  void toggleEnabled() { setEnabled(!enabled_); }
  void setEnabled(bool enabled) {
    enabled_ = enabled;
    /*
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
    */
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

ButtonColors idleCol = {BLACK, WHITE, WHITE};
ButtonColors idleEnabledCol = {WHITE, BLACK, WHITE};
ButtonColors pressedCol = {RED, WHITE, WHITE};
Button nextButton(/*x=*/160, /*y=*/0, /*w=*/160, /*h=*/60, /*rot1=*/false, "Next", idleCol, pressedCol);
Button loopButton(/*x=*/160, /*y=*/60, /*w=*/160, /*h=*/60, /*rot1=*/false, "Loop", idleCol, pressedCol);
Button patternControlButton(/*x=*/0, /*y=*/120, /*w=*/160, /*h=*/120, /*rot1=*/false,
                            "Pattern Control", idleCol, pressedCol,
                            BUTTON_DATUM, /*dx=*/0, /*dy=*/-25);
Button backButton(/*x=*/0, /*y=*/0, /*w=*/160, /*h=*/60, /*rot1=*/false, "Back", idleCol, pressedCol);
Button downButton(/*x=*/80, /*y=*/60, /*w=*/80, /*h=*/60, /*rot1=*/false, "Down", idleCol, pressedCol);
Button upButton(/*x=*/0, /*y=*/60, /*w=*/80, /*h=*/60, /*rot1=*/false, "Up", idleCol, pressedCol);
Button overrideButton(/*x=*/0, /*y=*/120, /*w=*/160, /*h=*/60, /*rot1=*/false, "Override", idleCol, pressedCol);
Button confirmButton(/*x=*/0, /*y=*/180, /*w=*/160, /*h=*/60, /*rot1=*/false, "Confirm", idleCol, pressedCol);
std::string currentPatternName;

void setupButtonsDrawZone() {
  constexpr int16_t kButtonDrawOffset = 3;
  for (Button* b : Button::instances) {
    b->drawZone = ::Zone(/*x=*/b->x + kButtonDrawOffset,
                         /*y=*/b->y + kButtonDrawOffset,
                         /*w=*/b->w - 2 * kButtonDrawOffset,
                         /*h=*/b->h - 2 * kButtonDrawOffset);
  }
}

void drawPatternControlButton(Button& b, ButtonColors bc) {
  if (gScreenMode != ScreenMode::kMainMenu) { return; }
  info("drawing pattern control button bg 0x%x outline 0x%x text 0x%x",
       bc.bg, bc.outline, bc.text);
  // First call default draw function to draw button text, outline, and background.
  M5Buttons::drawFunction(b, bc);
  // Then add custom pattern string.
  M5.Lcd.setTextColor(bc.text, bc.bg);
  M5.Lcd.setTextDatum(BC_DATUM);  // Bottom Center.
  M5.Lcd.drawString(currentPatternName.c_str(), /*x=*/80, /*y=*/210);
}

void drawMainMenuButtons() {
  info("drawMainMenuButtons");
  nextButton.draw();
  loopButton.draw();
  patternControlButton.draw();
}

void hideMainMenuButtons() {
  info("hideMainMenuButtons");
  nextButton.hide();
  loopButton.hide();
  patternControlButton.hide();
}

void drawPatternControlMenuButtons() {
  info("drawPatternControlMenuButtons");
  backButton.draw();
  downButton.draw();
  upButton.draw();
  overrideButton.draw();
  confirmButton.draw();
}

void hidePatternControlMenuButtons() {
  info("hidePatternControlMenuButtons");
  backButton.hide();
  downButton.hide();
  upButton.hide();
  overrideButton.hide();
  confirmButton.hide();
}

class PatternControlMenu {
 public:
  void draw() {
    // Reset text datum and color in case we need to draw any.
    M5.Lcd.setTextDatum(TL_DATUM);  // Top Left.
    M5.Lcd.setTextColor(WHITE, BLACK);
    switch (state_) {
    case State::kOff:  // Fall through.
    case State::kPattern: {
      state_ = State::kPattern;
      M5.Lcd.fillRect(x_, /*y=*/0, /*w=*/155, /*h=*/240, BLACK);
      if (selectedIndex_ < kNumRegularPatterns) {
        for (uint8_t i = 0; i < kNumRegularPatterns; i++) {
          drawPatternTextLine(i,
                              kSelectablePatterns[i].name,
                              i == selectedIndex_);
        }
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.drawString("Special Patterns...", x_, kNumRegularPatterns * dy());
      } else {
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.drawString("Regular Patterns...", x_, /*y=*/0);
        for (uint8_t i = 0; i < kNumSpecialPatterns; i++) {
          drawPatternTextLine(i + 1,
                              kSelectablePatterns[i + kNumRegularPatterns].name,
                              i + kNumRegularPatterns == selectedIndex_);
        }
      }
    } break;
    }
  }
  void downPressed() {
    if (selectedIndex_ < kNumRegularPatterns - 1) {
      drawPatternTextLine(selectedIndex_, kSelectablePatterns[selectedIndex_].name, /*selected=*/false);
      selectedIndex_++;
      drawPatternTextLine(selectedIndex_, kSelectablePatterns[selectedIndex_].name, /*selected=*/true);
    } else if (selectedIndex_ == kNumRegularPatterns - 1) {
      selectedIndex_ ++;
      draw();
    } else if (selectedIndex_ < kNumRegularPatterns + kNumSpecialPatterns - 1) {
      drawPatternTextLine(1 + selectedIndex_ - kNumRegularPatterns,
                          kSelectablePatterns[selectedIndex_].name,
                          /*selected=*/false);
      selectedIndex_++;
      drawPatternTextLine(1 + selectedIndex_ - kNumRegularPatterns,
                          kSelectablePatterns[selectedIndex_].name,
                          /*selected=*/true);
    }
  }
  void upPressed() {
    if (selectedIndex_ == 0) {
      // Do nothing.
    } else if (selectedIndex_ < kNumRegularPatterns) {
      drawPatternTextLine(selectedIndex_,
                          kSelectablePatterns[selectedIndex_].name,
                          /*selected=*/false);
      selectedIndex_--;
      drawPatternTextLine(selectedIndex_,
                          kSelectablePatterns[selectedIndex_].name,
                          /*selected=*/true);
    } else if (selectedIndex_ == kNumRegularPatterns) {
      selectedIndex_--;
      draw();
    } else {
      drawPatternTextLine(1 + selectedIndex_ - kNumRegularPatterns,
                          kSelectablePatterns[selectedIndex_].name,
                          /*selected=*/false);
      selectedIndex_--;
      drawPatternTextLine(1 + selectedIndex_ - kNumRegularPatterns,
                          kSelectablePatterns[selectedIndex_].name,
                          /*selected=*/true);
    }

  }
  void backPressed() {
    state_ = State::kOff;
  }
  void overridePressed() {
    overrideEnabled_ = !overrideEnabled_;
    overrideButton.off = overrideEnabled_ ? idleEnabledCol : idleCol;
    overrideButton.setLabel(overrideEnabled_ ? "Override ON" : "Override");
  }
 private:
  uint8_t dy() {
    if (dy_ == 0) { dy_ = M5.Lcd.fontHeight(); }  // By default this is 22.
    return dy_;
  }
  void drawPatternTextLine(uint8_t i, const char* text, bool selected) {
    M5.Lcd.setTextDatum(TL_DATUM);  // Top Left.
    const uint16_t y = i * dy();
    const uint16_t textColor = selected ? BLACK : WHITE;
    const uint16_t backgroundColor = selected ? WHITE : BLACK;
    M5.Lcd.setTextColor(textColor, backgroundColor);
    M5.Lcd.fillRect(x_, y, /*w=*/155, /*h=*/dy(), backgroundColor);
    M5.Lcd.drawString(text, x_, y);
  }
  enum class State {
    kOff,
    kPattern,
    kPalette,
    kColor,
    kConfirmed,
  };
  struct SelectablePattern {
    const char* name;
    PatternBits bits;
    State nextState;
  };
  static constexpr uint8_t kNumRegularPatterns = 5 + 4;
  static constexpr uint8_t kNumSpecialPatterns = 3 + 2;
  SelectablePattern kSelectablePatterns[kNumRegularPatterns + kNumSpecialPatterns] = {
    // Non-palette regular patterns.
    {"flame",       0x60000001, State::kConfirmed},
    {"glitter",     0x40000001, State::kConfirmed},
    {"the-matrix",  0x30000001, State::kConfirmed},
    {"threesine",   0x20000001, State::kConfirmed},
    {"raibow",      0x00000001, State::kConfirmed},
    // Palette regular patterns.
    {"spin-plasma", 0xE0000001, State::kPalette},
    {"hiphotic",    0xC0000001, State::kPalette},
    {"metaballs",   0xA0000001, State::kPalette},
    {"bursts",      0x80000001, State::kPalette},
    // Non-color special patterns.
    {"synctest",      0x0F0000, State::kConfirmed},
    {"calibration",   0x100000, State::kConfirmed},
    {"follow-strand", 0x110000, State::kConfirmed},
    // Color special patterns.
    {"solid",          0x00000, State::kConfirmed},
    {"glow",           0x70000, State::kConfirmed},
    /*
    {"black",          0x00000, State::kConfirmed},
    {"red",            0x10000, State::kConfirmed},
    {"green",          0x20000, State::kConfirmed},
    {"blue",           0x30000, State::kConfirmed},
    {"purple",         0x40000, State::kConfirmed},
    {"cyan",           0x50000, State::kConfirmed},
    {"yellow",         0x60000, State::kConfirmed},
    {"white",          0x70000, State::kConfirmed},
    {"glow-red",       0x80000, State::kConfirmed},
    {"glow-green",     0x90000, State::kConfirmed},
    {"glow-blue",      0xA0000, State::kConfirmed},
    {"glow-purple",    0xB0000, State::kConfirmed},
    {"glow-cyan",      0xC0000, State::kConfirmed},
    {"glow-yellow",    0xD0000, State::kConfirmed},
    {"glow-white",     0xE0000, State::kConfirmed},
    */
  };
  State state_ = State::kOff;
  // SelectablePattern selectablePattern = SelectablePattern("flame", 0x60000001, State::kConfirmed);
  uint8_t selectedIndex_ = 0;
  const int16_t x_ = 165;
  uint8_t dy_ = 0;
  bool overrideEnabled_ = false;
};
PatternControlMenu gPatternControlMenu;


#endif  // CORE2AWS

void vestSetup(void) {
  Serial.begin(115200);
#if CORE2AWS
  M5.begin(/*LCDEnable=*/true, /*SDEnable=*/false,
           /*SerialEnable=*/false, /*I2CEnable=*/false,
           /*mode=*/kMBusModeOutput);
  // TODO switch mode to kMBusModeInput once we power from pins instead of USB.
  M5.Lcd.fillScreen(BLACK);
  hidePatternControlMenuButtons();
  patternControlButton.drawFn = drawPatternControlButton;
  setupButtonsDrawZone();
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
#if CORE2AWS
  currentPatternName = player.currentEffectName();
  drawMainMenuButtons();
#endif  // CORE2AWS
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
    ::Point pressed = M5.background.event.from;
    int16_t px = pressed.x;
    int16_t py = pressed.y;
    info("%u background pressed x=%d y=%d, free RAM %u/%u free PSRAM %u/%u",
         currentTime, px, py, freeHeap, totalHeap, freePSRAM, totalPSRAM);
    switch (gScreenMode) {
    case ScreenMode::kMainMenu: {
        gScreenMode = ScreenMode::kFullScreenPattern;
      if (px < 160 && py < 120) {
        info("%u pattern screen pressed", currentTime);
        hideMainMenuButtons();
        M5.Lcd.fillScreen(BLACK);
        core2ScreenRenderer.setFullScreen(true);
      }
    } break;
    case ScreenMode::kFullScreenPattern: {
      gScreenMode = ScreenMode::kMainMenu;
      info("%u full screen pattern pressed", currentTime);
      core2ScreenRenderer.setFullScreen(false);
      M5.Lcd.fillScreen(BLACK);
      drawMainMenuButtons();
    } break;
    case ScreenMode::kPatternControlMenu: {
    } break;
    case ScreenMode::kSystemMenu: {
    } break;
    //core2ScreenRenderer.toggleEnabled();
    }
  }
  if (nextButton.wasPressed() && gScreenMode == ScreenMode::kMainMenu) {
    info("%u next pressed", currentTime);
    player.next(currentTime);
  }
  if (loopButton.wasPressed() && gScreenMode == ScreenMode::kMainMenu) {
    info("%u loop pressed", currentTime);
    if (player.isLooping()) {
      player.stopLooping(currentTime);
      loopButton.setLabel("Loop");
      loopButton.off = idleCol;
    } else {
      player.loopOne(currentTime);
      loopButton.setLabel("Looping");
      loopButton.off = idleEnabledCol;
    }
    loopButton.draw();
  }
  if (patternControlButton.wasPressed() && gScreenMode == ScreenMode::kMainMenu) {
    gScreenMode = ScreenMode::kPatternControlMenu;
    info("%u pattern control button pressed", currentTime);
    hideMainMenuButtons();
    core2ScreenRenderer.setEnabled(false);
    M5.Lcd.fillScreen(BLACK);
    drawPatternControlMenuButtons();
    gPatternControlMenu.draw();
  }
  if (backButton.wasPressed() &&
      (gScreenMode == ScreenMode::kPatternControlMenu || gScreenMode == ScreenMode::kSystemMenu)) {
    gScreenMode = ScreenMode::kMainMenu;
    info("%u back button pressed", currentTime);
    gPatternControlMenu.backPressed();
    hidePatternControlMenuButtons();
    M5.Lcd.fillScreen(BLACK);
    drawMainMenuButtons();
    core2ScreenRenderer.setEnabled(true);
  }
  if (downButton.wasPressed() && gScreenMode == ScreenMode::kPatternControlMenu) {
    gPatternControlMenu.downPressed();
  }
  if (upButton.wasPressed() && gScreenMode == ScreenMode::kPatternControlMenu) {
    gPatternControlMenu.upPressed();
  }
  if (overrideButton.wasPressed() && gScreenMode == ScreenMode::kPatternControlMenu) {
    gPatternControlMenu.overridePressed();
  }
  // std::string patternControlLabel = "Pattern 2Control\n3";
  // patternControlLabel += player.currentEffectName();
  // patternControlButton.setLabel(patternControlLabel.c_str());
  // patternControlButton.draw();
  std::string patternName = player.currentEffectName();
  if (patternName != currentPatternName) {
    currentPatternName = patternName;
    if (gScreenMode == ScreenMode::kMainMenu) {
      info("%u drawing new pattern name in pattern control button", currentTime);
      patternControlButton.draw();
    }
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

  if (!player.render(currentTime)) {
    return;
  }
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
