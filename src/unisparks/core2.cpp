#include <Unisparks.h>

#if WEARABLE && CORE2AWS

#include <M5Core2.h>

namespace unisparks {

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
// TODO split the player in half so we can render the selected pattern in the right half of the Back button.
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

void drawButtonButNotInMainMenu(Button& b, ButtonColors bc) {
  if (gScreenMode != ScreenMode::kMainMenu) {
    M5Buttons::drawFunction(b, bc);
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
    info("PatternControlMenu::draw()");
    // Reset text datum and color in case we need to draw any.
    M5.Lcd.setTextDatum(TL_DATUM);  // Top Left.
    M5.Lcd.setTextColor(WHITE, BLACK);
    switch (state_) {
    case State::kOff:  // Fall through.
    case State::kPattern: {
      state_ = State::kPattern;
      M5.Lcd.fillRect(x_, /*y=*/0, /*w=*/155, /*h=*/240, BLACK);
      if (selectedPatternIndex_ < kNumRegularPatterns) {
        for (uint8_t i = 0; i < kNumRegularPatterns; i++) {
          drawPatternTextLine(i,
                              kSelectablePatterns[i].name,
                              i == selectedPatternIndex_);
        }
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.drawString("Special Patterns...", x_, kNumRegularPatterns * dy());
      } else {
        M5.Lcd.setTextColor(WHITE, BLACK);
        M5.Lcd.drawString("Regular Patterns...", x_, /*y=*/0);
        for (uint8_t i = 0; i < kNumSpecialPatterns; i++) {
          drawPatternTextLine(i + 1,
                              kSelectablePatterns[i + kNumRegularPatterns].name,
                              i + kNumRegularPatterns == selectedPatternIndex_);
        }
      }
    } break;
    }
    drawConfirmButton();
  }
  void downPressed() {
    if (selectedPatternIndex_ < kNumRegularPatterns - 1) {
      drawPatternTextLine(selectedPatternIndex_, kSelectablePatterns[selectedPatternIndex_].name, /*selected=*/false);
      selectedPatternIndex_++;
      drawPatternTextLine(selectedPatternIndex_, kSelectablePatterns[selectedPatternIndex_].name, /*selected=*/true);
      drawConfirmButton();
    } else if (selectedPatternIndex_ == kNumRegularPatterns - 1) {
      selectedPatternIndex_ ++;
      draw();
    } else if (selectedPatternIndex_ < kNumRegularPatterns + kNumSpecialPatterns - 1) {
      drawPatternTextLine(1 + selectedPatternIndex_ - kNumRegularPatterns,
                          kSelectablePatterns[selectedPatternIndex_].name,
                          /*selected=*/false);
      selectedPatternIndex_++;
      drawPatternTextLine(1 + selectedPatternIndex_ - kNumRegularPatterns,
                          kSelectablePatterns[selectedPatternIndex_].name,
                          /*selected=*/true);
      drawConfirmButton();
    }
  }
  void upPressed() {
    if (selectedPatternIndex_ == 0) {
      // Do nothing.
    } else if (selectedPatternIndex_ < kNumRegularPatterns) {
      drawPatternTextLine(selectedPatternIndex_,
                          kSelectablePatterns[selectedPatternIndex_].name,
                          /*selected=*/false);
      selectedPatternIndex_--;
      drawPatternTextLine(selectedPatternIndex_,
                          kSelectablePatterns[selectedPatternIndex_].name,
                          /*selected=*/true);
      drawConfirmButton();
    } else if (selectedPatternIndex_ == kNumRegularPatterns) {
      selectedPatternIndex_--;
      draw();
    } else {
      drawPatternTextLine(1 + selectedPatternIndex_ - kNumRegularPatterns,
                          kSelectablePatterns[selectedPatternIndex_].name,
                          /*selected=*/false);
      selectedPatternIndex_--;
      drawPatternTextLine(1 + selectedPatternIndex_ - kNumRegularPatterns,
                          kSelectablePatterns[selectedPatternIndex_].name,
                          /*selected=*/true);
      drawConfirmButton();
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
  bool confirmPressed(Player& player, Milliseconds currentTime) {
    const char* confirmLabel;
    if (state_ == State::kPattern) {
      State nextState = kSelectablePatterns[selectedPatternIndex_].nextState;
      if (nextState == State::kPalette || nextState == State::kColor) {
        info("Pattern %s confirmed now asking for %s", (nextState == State::kPalette ? "palette" : "color"));
        state_ = nextState;
        draw();
      } else {
        info("Pattern %s confirmed now playing 0x%4x",
             kSelectablePatterns[selectedPatternIndex_].name, kSelectablePatterns[selectedPatternIndex_].bits);
        setPattern(player, kSelectablePatterns[selectedPatternIndex_].bits, currentTime);
        return true;
      }
    }
    return false;
  }
 private:
  void setPattern(Player& player, PatternBits patternBits, Milliseconds currentTime) {
    patternBits = randomizePatternBits(patternBits);
    player.setPattern(patternBits, currentTime);
  }
  void drawConfirmButton() {
    const char* confirmLabel;
    switch (kSelectablePatterns[selectedPatternIndex_].nextState) {
      case State::kOff:       confirmLabel = "Error Off"; break;
      case State::kPattern:   confirmLabel = "Error Pattern"; break;
      case State::kPalette:   confirmLabel = "Select Palette"; break;
      case State::kColor:     confirmLabel = "Select Color"; break;
      case State::kConfirmed: confirmLabel = "Confirm"; break;
    }
    confirmButton.setLabel(confirmLabel);
    info("drawing confirm button with label %s", confirmLabel);
    confirmButton.draw();
  }
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
    {"rainbow",      0x00000001, State::kConfirmed},
    // Palette regular patterns.
    {"spin-plasma", 0xE0000001, State::kPalette},
    {"hiphotic",    0xC0000001, State::kPalette},
    {"metaballs",   0xA0000001, State::kPalette},
    {"bursts",      0x80000001, State::kPalette},
    // TODO add a mode that forces looping through all patterns but with a fixed palette.
    // Non-color special patterns.
    {"synctest",      0x0F0000, State::kConfirmed},
    {"calibration",   0x100000, State::kConfirmed},
    {"follow-strand", 0x110000, State::kConfirmed},
    // Color special patterns.
    {"solid",          0x00000, State::kColor},
    {"glow",           0x70000, State::kColor},
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
  const char* kPaletteNames[7] = {
    "heat",
    "lava",
    "ocean",
    "cloud",
    "party",
    "forest",
    "rainbow",
  };
  State state_ = State::kOff;
  // SelectablePattern selectablePattern = SelectablePattern("flame", 0x60000001, State::kConfirmed);
  uint8_t selectedPatternIndex_ = 0;
  uint8_t selectedPaletteIndex_ = 0;
  uint8_t selectedColorIndex_ = 0;
  const int16_t x_ = 165;
  uint8_t dy_ = 0;
  bool overrideEnabled_ = false;
};
PatternControlMenu gPatternControlMenu;

void core2SetupStart(Player& player) {
  M5.begin(/*LCDEnable=*/true, /*SDEnable=*/false,
           /*SerialEnable=*/false, /*I2CEnable=*/false,
           /*mode=*/kMBusModeOutput);
  // TODO switch mode to kMBusModeInput once we power from pins instead of USB.
  M5.Lcd.fillScreen(BLACK);
  hidePatternControlMenuButtons();
  patternControlButton.drawFn = drawPatternControlButton;
  backButton.drawFn = drawButtonButNotInMainMenu;
  confirmButton.drawFn = drawButtonButNotInMainMenu;
  setupButtonsDrawZone();
  player.addStrand(core2ScreenPixels, core2ScreenRenderer);
}

void core2SetupEnd(Player& player) {
  currentPatternName = player.currentEffectName();
  drawMainMenuButtons();
}

void core2Loop(Player& player, Milliseconds currentTime) {
  M5.Touch.update();
  M5.Buttons.update();
  if (M5.background.wasPressed()) {
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t totalHeap = ESP.getHeapSize();
    // TODO enable PSRAM by adding "-DBOARD_HAS_PSRAM" and "-mfix-esp32-psram-cache-issue" to build_flags in platformio.ini
    // https://thingpulse.com/esp32-how-to-use-psram/
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
  if (confirmButton.wasPressed() && gScreenMode == ScreenMode::kPatternControlMenu) {
    if (gPatternControlMenu.confirmPressed(player, currentTime)) {
      gScreenMode = ScreenMode::kMainMenu;
      hidePatternControlMenuButtons();
      M5.Lcd.fillScreen(BLACK);
      drawMainMenuButtons();
      core2ScreenRenderer.setEnabled(true);
    }
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
}

}  // namespace unisparks

#endif  // CORE2AWS && WEARABLE
