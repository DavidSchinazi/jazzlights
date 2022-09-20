#include <Jazzlights.h>

#if WEARABLE && CORE2AWS

#include <M5Core2.h>

namespace jazzlights {

constexpr uint8_t kDefaultOnBrightness = 12;
constexpr uint8_t kMinOnBrightness = 1;
constexpr uint8_t kMaxOnBrightness = 20;
uint8_t gOnBrightness = kDefaultOnBrightness;
void setCore2ScreenBrightness(uint8_t brightness, bool allowUnsafe = false) {
  info("Setting screen brightness to %u%s", brightness, (allowUnsafe ? " (unsafe enabled)" : ""));
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
  kOff,
  kLocked1,
  kLocked2,
  kMainMenu,
  kFullScreenPattern,
  kPatternControlMenu,
  kSystemMenu,
};
ScreenMode gScreenMode =
#if BUTTON_LOCK
    ScreenMode::kOff;
#else   // BUTTON_LOCK
    ScreenMode::kMainMenu;
#endif  // BUTTON_LOCK

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
      uint16_t color16 =
          ((uint16_t)(rgb.red & 0xF8) << 8) | ((uint16_t)(rgb.green & 0xFC) << 3) | ((rgb.blue & 0xF8) >> 3);
      int32_t x = i % 40;
      int32_t y = i / 40;
      int32_t factor = fullScreen_ ? 8 : 4;
      for (size_t xi = 0; xi < factor; xi++) {
        for (size_t yi = 0; yi < factor; yi++) { rowColors16[x * factor + xi + yi * 40 * factor] = color16; }
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
Button patternControlButton(/*x=*/0, /*y=*/120, /*w=*/160, /*h=*/120, /*rot1=*/false, "Pattern Control", idleCol,
                            pressedCol, BUTTON_DATUM, /*dx=*/0, /*dy=*/-25);
Button systemButton(/*x=*/160, /*y=*/120, /*w=*/160, /*h=*/120, /*rot1=*/false, "System", idleCol, pressedCol,
                    BUTTON_DATUM, /*dx=*/0, /*dy=*/-25);
Button backButton(/*x=*/0, /*y=*/0, /*w=*/160, /*h=*/60, /*rot1=*/false, "Back", idleCol, pressedCol);
// TODO split the player in half so we can render the selected pattern in the right half of the Back button.
Button downButton(/*x=*/80, /*y=*/60, /*w=*/80, /*h=*/60, /*rot1=*/false, "Down", idleCol, pressedCol);
Button upButton(/*x=*/0, /*y=*/60, /*w=*/80, /*h=*/60, /*rot1=*/false, "Up", idleCol, pressedCol);
Button overrideButton(/*x=*/0, /*y=*/120, /*w=*/160, /*h=*/60, /*rot1=*/false, "Override", idleCol, pressedCol);
Button confirmButton(/*x=*/0, /*y=*/180, /*w=*/160, /*h=*/60, /*rot1=*/false, "Confirm", idleCol, pressedCol);
Button lockButton(/*x=*/160, /*y=*/180, /*w=*/160, /*h=*/60, /*rot1=*/false, "Lock", idleCol, pressedCol);
Button shutdownButton(/*x=*/0, /*y=*/180, /*w=*/160, /*h=*/60, /*rot1=*/false, "Shutdown", idleCol, pressedCol);
Button unlock1Button(/*x=*/160, /*y=*/0, /*w=*/160, /*h=*/60, /*rot1=*/false, "Unlock", idleCol, pressedCol);
Button unlock2Button(/*x=*/0, /*y=*/180, /*w=*/160, /*h=*/60, /*rot1=*/false, "Unlock", idleCol, pressedCol);
Button ledMinusButton(/*x=*/160, /*y=*/0, /*w=*/80, /*h=*/60, /*rot1=*/false, "LED-", idleCol, pressedCol);
Button ledPlusButton(/*x=*/240, /*y=*/0, /*w=*/80, /*h=*/60, /*rot1=*/false, "LED+", idleCol, pressedCol);
Button screenMinusButton(/*x=*/160, /*y=*/60, /*w=*/80, /*h=*/60, /*rot1=*/false, "Screen-", idleCol, pressedCol);
Button screenPlusButton(/*x=*/240, /*y=*/60, /*w=*/80, /*h=*/60, /*rot1=*/false, "Screen+", idleCol, pressedCol);
std::string gCurrentPatternName;
constexpr Milliseconds kLockDelay = 60000;
constexpr Milliseconds kUnlockingTime = 5000;
Milliseconds gLastScreenInteractionTime = -1;

#if defined(FIRST_BRIGHTNESS) && FIRST_BRIGHTNESS == 0
uint8_t gLedBrightness = 2;
#else
uint8_t gLedBrightness = 32;
#endif

uint8_t getBrightness() { return gLedBrightness; }

void setupButtonsDrawZone() {
  constexpr int16_t kButtonDrawOffset = 3;
  for (Button* b : Button::instances) {
    b->drawZone = ::Zone(/*x=*/b->x + kButtonDrawOffset,
                         /*y=*/b->y + kButtonDrawOffset,
                         /*w=*/b->w - 2 * kButtonDrawOffset,
                         /*h=*/b->h - 2 * kButtonDrawOffset);
  }
}

void drawButtonOnlyInSubMenus(Button& b, ButtonColors bc) {
  if (gScreenMode != ScreenMode::kOff && gScreenMode != ScreenMode::kLocked1 && gScreenMode != ScreenMode::kLocked2 &&
      gScreenMode != ScreenMode::kMainMenu) {
    M5Buttons::drawFunction(b, bc);
  }
}

void drawButtonButOnlyInLocked1(Button& b, ButtonColors bc) {
  if (gScreenMode == ScreenMode::kLocked1) { M5Buttons::drawFunction(b, bc); }
}

void drawButtonButOnlyInLocked2(Button& b, ButtonColors bc) {
  if (gScreenMode == ScreenMode::kLocked2) { M5Buttons::drawFunction(b, bc); }
}

void drawPatternControlButton(Button& b, ButtonColors bc) {
  if (gScreenMode != ScreenMode::kMainMenu) { return; }
  info("drawing pattern control button bg 0x%x outline 0x%x text 0x%x", bc.bg, bc.outline, bc.text);
  // First call default draw function to draw button text, outline, and background.
  M5Buttons::drawFunction(b, bc);
  // Then add custom pattern string.
  M5.Lcd.setTextColor(bc.text, bc.bg);
  M5.Lcd.setTextDatum(BC_DATUM);  // Bottom Center.
  M5.Lcd.drawString(gCurrentPatternName.c_str(), /*x=*/80, /*y=*/210);
}

void drawSystemButton(Button& b, ButtonColors bc) {
  if (gScreenMode != ScreenMode::kMainMenu) { return; }
  info("drawing system button bg 0x%x outline 0x%x text 0x%x", bc.bg, bc.outline, bc.text);
  // First call default draw function to draw button text, outline, and background.
  M5Buttons::drawFunction(b, bc);
  // TODO add custom system info.
}

void drawMainMenuButtons() {
  info("drawMainMenuButtons");
  unlock1Button.hide();
  unlock2Button.hide();
  nextButton.draw();
  loopButton.draw();
  patternControlButton.draw();
  systemButton.draw();
}

void hideMainMenuButtons() {
  info("hideMainMenuButtons");
  nextButton.hide();
  loopButton.hide();
  patternControlButton.hide();
  systemButton.hide();
}

void drawPatternControlMenuButtons() {
  info("drawPatternControlMenuButtons");
  unlock1Button.hide();
  unlock2Button.hide();
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

void drawSystemMenuButtons() {
  info("drawSystemMenuButtons");
  unlock1Button.hide();
  unlock2Button.hide();
  backButton.draw();
  lockButton.draw();
  shutdownButton.draw();
  ledPlusButton.draw();
  ledMinusButton.draw();
  screenPlusButton.draw();
  screenMinusButton.draw();
}

void hideSystemMenuButtons() {
  info("hideSystemMenuButtons");
  backButton.hide();
  lockButton.hide();
  shutdownButton.hide();
  ledPlusButton.hide();
  ledMinusButton.hide();
  screenPlusButton.hide();
  screenMinusButton.hide();
}

void setDefaultPrecedence(Player& player, Milliseconds currentTime) {
  player.updatePrecedence(4000, 1000, currentTime);
}

void setOverridePrecedence(Player& player, Milliseconds currentTime) {
  player.updatePrecedence(36000, 5000, currentTime);
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
            drawPatternTextLine(i, kSelectablePatterns[i].name, i == selectedPatternIndex_);
          }
          M5.Lcd.setTextColor(WHITE, BLACK);
          M5.Lcd.drawString("Special Patterns...", x_, kNumRegularPatterns * dy());
        } else {
          M5.Lcd.setTextColor(WHITE, BLACK);
          M5.Lcd.drawString("Regular Patterns...", x_, /*y=*/0);
          for (uint8_t i = 0; i < kNumSpecialPatterns; i++) {
            drawPatternTextLine(i + 1, kSelectablePatterns[i + kNumRegularPatterns].name,
                                i + kNumRegularPatterns == selectedPatternIndex_);
          }
        }
      } break;
      case State::kPalette: {
        M5.Lcd.fillRect(x_, /*y=*/0, /*w=*/155, /*h=*/240, BLACK);
        for (uint8_t i = 0; i < kNumPalettes; i++) {
          drawPatternTextLine(i, kPaletteNames[i], i == selectedPaletteIndex_);
        }
      } break;
      case State::kColor: {
        M5.Lcd.fillRect(x_, /*y=*/0, /*w=*/155, /*h=*/240, BLACK);
        for (uint8_t i = 0; i < kNumColors; i++) { drawPatternTextLine(i, kColorNames[i], i == selectedColorIndex_); }
      } break;
    }
    drawConfirmButton();
  }
  void downPressed() {
    if (state_ == State::kPattern) {
      if (selectedPatternIndex_ < kNumRegularPatterns - 1) {
        drawPatternTextLine(selectedPatternIndex_, kSelectablePatterns[selectedPatternIndex_].name, /*selected=*/false);
        selectedPatternIndex_++;
        drawPatternTextLine(selectedPatternIndex_, kSelectablePatterns[selectedPatternIndex_].name, /*selected=*/true);
        drawConfirmButton();
      } else if (selectedPatternIndex_ == kNumRegularPatterns - 1) {
        selectedPatternIndex_++;
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
    } else if (state_ == State::kPalette) {
      if (selectedPaletteIndex_ < kNumPalettes - 1) {
        drawPatternTextLine(selectedPaletteIndex_, kPaletteNames[selectedPaletteIndex_], /*selected=*/false);
        selectedPaletteIndex_++;
        drawPatternTextLine(selectedPaletteIndex_, kPaletteNames[selectedPaletteIndex_], /*selected=*/true);
      }
    } else if (state_ == State::kColor) {
      if (selectedColorIndex_ < kNumColors - 1) {
        drawPatternTextLine(selectedColorIndex_, kColorNames[selectedColorIndex_], /*selected=*/false);
        selectedColorIndex_++;
        drawPatternTextLine(selectedColorIndex_, kColorNames[selectedColorIndex_], /*selected=*/true);
      }
    }
  }
  void upPressed() {
    if (state_ == State::kPattern) {
      if (selectedPatternIndex_ == 0) {
        // Do nothing.
      } else if (selectedPatternIndex_ < kNumRegularPatterns) {
        drawPatternTextLine(selectedPatternIndex_, kSelectablePatterns[selectedPatternIndex_].name,
                            /*selected=*/false);
        selectedPatternIndex_--;
        drawPatternTextLine(selectedPatternIndex_, kSelectablePatterns[selectedPatternIndex_].name,
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
    } else if (state_ == State::kPalette) {
      if (selectedPaletteIndex_ > 0) {
        drawPatternTextLine(selectedPaletteIndex_, kPaletteNames[selectedPaletteIndex_], /*selected=*/false);
        selectedPaletteIndex_--;
        drawPatternTextLine(selectedPaletteIndex_, kPaletteNames[selectedPaletteIndex_], /*selected=*/true);
      }
    } else if (state_ == State::kColor) {
      if (selectedColorIndex_ > 0) {
        drawPatternTextLine(selectedColorIndex_, kColorNames[selectedColorIndex_], /*selected=*/false);
        selectedColorIndex_--;
        drawPatternTextLine(selectedColorIndex_, kColorNames[selectedColorIndex_], /*selected=*/true);
      }
    }
  }
  bool backPressed() {
    if (state_ == State::kPattern) {
      state_ = State::kOff;
      return true;
    }
    state_ = State::kPattern;
    draw();
    return false;
  }
  void overridePressed(Player& player, Milliseconds currentTime) {
    overrideEnabled_ = !overrideEnabled_;
    overrideButton.off = overrideEnabled_ ? idleEnabledCol : idleCol;
    overrideButton.setLabel(overrideEnabled_ ? "Override ON" : "Override");
    if (overrideEnabled_) {
      setOverridePrecedence(player, currentTime);
    } else {
      setDefaultPrecedence(player, currentTime);
    }
  }
  bool confirmPressed(Player& player, Milliseconds currentTime) {
    const char* confirmLabel;
    if (state_ == State::kPattern) {
      State nextState = kSelectablePatterns[selectedPatternIndex_].nextState;
      if (nextState == State::kPalette || nextState == State::kColor) {
        info("%u Pattern %s confirmed now asking for %s", currentTime, kSelectablePatterns[selectedPatternIndex_].name,
             (nextState == State::kPalette ? "palette" : "color"));
        state_ = nextState;
        draw();
      } else {
        info("%u Pattern %s confirmed now playing", currentTime, kSelectablePatterns[selectedPatternIndex_].name);
        player.stopForcePalette(currentTime);
        return setPattern(player, kSelectablePatterns[selectedPatternIndex_].bits, currentTime);
      }
    } else if (state_ == State::kPalette) {
      info("%u Pattern %s and palette %s confirmed now playing", currentTime,
           kSelectablePatterns[selectedPatternIndex_].name, kPaletteNames[selectedPaletteIndex_]);
      return setPatternWithPalette(player, kSelectablePatterns[selectedPatternIndex_].bits, selectedPaletteIndex_,
                                   currentTime);
    } else if (state_ == State::kColor) {
      info("%u Pattern %s and color %s confirmed now playing", currentTime,
           kSelectablePatterns[selectedPatternIndex_].name, kColorNames[selectedColorIndex_]);
      return setPatternWithColor(player, kSelectablePatterns[selectedPatternIndex_].bits, selectedColorIndex_,
                                 currentTime);
    }
    return false;
  }

 private:
  bool setPattern(Player& player, PatternBits patternBits, Milliseconds currentTime) {
    patternBits = randomizePatternBits(patternBits);
    player.setPattern(patternBits, currentTime);
    state_ = State::kOff;
    return true;
  }
  bool setPatternWithPalette(Player& player, PatternBits patternBits, uint8_t palette, Milliseconds currentTime) {
    if (patternBits == 0x00FF0000) {  // forced palette.
      player.forcePalette(palette, currentTime);
      state_ = State::kOff;
      return true;
    }
    player.stopForcePalette(currentTime);
    return setPattern(player, patternBits | (palette << 13), currentTime);
  }
  bool setPatternWithColor(Player& player, PatternBits patternBits, uint8_t color, Milliseconds currentTime) {
    player.stopForcePalette(currentTime);
    if (patternBits == 0x70000 && color == 0) {  // glow-black is just solid-black.
      return setPattern(player, 0, currentTime);
    }
    return setPattern(player, patternBits + color * 0x10000, currentTime);
  }
  void drawConfirmButton() {
    const char* confirmLabel;
    switch (kSelectablePatterns[selectedPatternIndex_].nextState) {
      case State::kOff: confirmLabel = "Error Off"; break;
      case State::kPattern: confirmLabel = "Error Pattern"; break;
      case State::kPalette: confirmLabel = "Select Palette"; break;
      case State::kColor: confirmLabel = "Select Color"; break;
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
  static constexpr uint8_t kNumSpecialPatterns = 3 + 2 + 1;
  SelectablePattern kSelectablePatterns[kNumRegularPatterns + kNumSpecialPatterns] = {
  // Non-palette regular patterns.
      {        "flame", 0x60000001, State::kConfirmed},
      {      "glitter", 0x40000001, State::kConfirmed},
      {   "the-matrix", 0x30000001, State::kConfirmed},
      {    "threesine", 0x20000001, State::kConfirmed},
      {      "rainbow", 0x00000001, State::kConfirmed},
 // Palette regular patterns.
      {  "spin-plasma", 0xE0000001,   State::kPalette},
      {     "hiphotic", 0xC0000001,   State::kPalette},
      {    "metaballs", 0xA0000001,   State::kPalette},
      {       "bursts", 0x80000001,   State::kPalette},
 // Non-color special patterns.
      {     "synctest",   0x0F0000, State::kConfirmed},
      {  "calibration",   0x100000, State::kConfirmed},
      {"follow-strand",   0x110000, State::kConfirmed},
 // Color special patterns.
      {        "solid",    0x00000,     State::kColor},
      {         "glow",    0x70000,     State::kColor},
 // All -palette pattern.
      {  "all-palette", 0x00FF0000,   State::kPalette},
  };
  static constexpr size_t kNumPalettes = 7;
  const char* kPaletteNames[kNumPalettes] = {
      "heat", "lava", "ocean", "cloud", "party", "forest", "rainbow",
  };
  static constexpr size_t kNumColors = 8;
  const char* kColorNames[kNumColors] = {
      "black", "red", "green", "blue", "purple", "cyan", "yellow", "white",
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

void core2SetupStart(Player& player, Milliseconds currentTime) {
  M5.begin(/*LCDEnable=*/true, /*SDEnable=*/false,
           /*SerialEnable=*/false, /*I2CEnable=*/false,
           /*mode=*/kMBusModeOutput);
  if (gScreenMode == ScreenMode::kOff) {
    setCore2ScreenBrightness(0);
  } else {
    setCore2ScreenBrightness(gOnBrightness);
  }
  // TODO switch mode to kMBusModeInput once we power from pins instead of USB.
  M5.Lcd.fillScreen(BLACK);
  hidePatternControlMenuButtons();
  hideSystemMenuButtons();
  unlock1Button.hide();
  unlock2Button.hide();
  patternControlButton.drawFn = drawPatternControlButton;
  systemButton.drawFn = drawSystemButton;
  backButton.drawFn = drawButtonOnlyInSubMenus;
  confirmButton.drawFn = drawButtonOnlyInSubMenus;
  lockButton.drawFn = drawButtonOnlyInSubMenus;
  shutdownButton.drawFn = drawButtonOnlyInSubMenus;
  unlock1Button.drawFn = drawButtonButOnlyInLocked1;
  unlock2Button.drawFn = drawButtonButOnlyInLocked2;
  setupButtonsDrawZone();
  player.addStrand(core2ScreenPixels, core2ScreenRenderer);
  setDefaultPrecedence(player, currentTime);
}

void startMainMenu(Player& player, Milliseconds currentTime) {
  gScreenMode = ScreenMode::kMainMenu;
  M5.Lcd.fillScreen(BLACK);
  drawMainMenuButtons();
  core2ScreenRenderer.setEnabled(true);
}

void core2SetupEnd(Player& player, Milliseconds currentTime) {
  gCurrentPatternName = player.currentEffectName();
  if (gScreenMode == ScreenMode::kMainMenu) {
    startMainMenu(player, currentTime);
  } else {
    hideMainMenuButtons();
    core2ScreenRenderer.setEnabled(false);
  }
}

void lockScreen(Milliseconds currentTime) {
  gLastScreenInteractionTime = -1;
  gScreenMode = ScreenMode::kOff;
  hideSystemMenuButtons();
  hideMainMenuButtons();
  hidePatternControlMenuButtons();
  core2ScreenRenderer.setEnabled(false);
  setCore2ScreenBrightness(0);
  M5.Lcd.fillScreen(BLACK);
}

void patternControlButtonPressed(Player& player, Milliseconds currentTime) {
  gScreenMode = ScreenMode::kPatternControlMenu;
  gLastScreenInteractionTime = currentTime;
  hideMainMenuButtons();
  core2ScreenRenderer.setEnabled(false);
  M5.Lcd.fillScreen(BLACK);
  drawPatternControlMenuButtons();
  gPatternControlMenu.draw();
}

void confirmButtonPressed(Player& player, Milliseconds currentTime) {
  gLastScreenInteractionTime = currentTime;
  if (gPatternControlMenu.confirmPressed(player, currentTime)) {
    hidePatternControlMenuButtons();
    startMainMenu(player, currentTime);
  }
}

void drawSystemTextLine(uint8_t i, const char* text) {
  constexpr uint8_t kSytemLineHeight = 22;
  constexpr uint8_t kSytemStartX = 5;
  constexpr uint8_t kSytemStartY = 65;
  M5.Lcd.setTextDatum(TL_DATUM);  // Top Left.
  const uint16_t x = kSytemStartX;
  const uint16_t y = kSytemStartY + i * kSytemLineHeight;
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.fillRect(x, y, /*w=*/155, /*h=*/kSytemLineHeight, BLACK);
  M5.Lcd.drawString(text, x, y);
}

void drawSystemTextLines(Player& player, Milliseconds currentTime) {
  size_t i = 0;
  char line[100] = {};
  // LED Brighness.
  snprintf(line, sizeof(line) - 1, "LED Brgt %u/255", gLedBrightness);
  drawSystemTextLine(i++, line);
  // Screen Brightness.
  snprintf(line, sizeof(line) - 1, "Screen Brgt %u/20", gOnBrightness);
  drawSystemTextLine(i++, line);
  // BLE.
  snprintf(line, sizeof(line) - 1, "BLE: %s", bleStatus(currentTime).c_str());
  drawSystemTextLine(i++, line);
  // Wi-Fi.
  snprintf(line, sizeof(line) - 1, "Wi-Fi: %s", wifiStatus(currentTime).c_str());
  drawSystemTextLine(i++, line);
  // Other.
  snprintf(line, sizeof(line) - 1, "%s", otherStatus(player, currentTime).c_str());
  drawSystemTextLine(i++, line);
}

void core2Loop(Player& player, Milliseconds currentTime) {
  M5.Touch.update();
  M5.Buttons.update();
  if (M5.background.wasPressed()) {
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t totalHeap = ESP.getHeapSize();
    // TODO enable PSRAM by adding "-DBOARD_HAS_PSRAM" and "-mfix-esp32-psram-cache-issue" to build_flags in
    // platformio.ini https://thingpulse.com/esp32-how-to-use-psram/
    uint32_t freePSRAM = ESP.getFreePsram();
    uint32_t totalPSRAM = ESP.getPsramSize();
    ::Point pressed = M5.background.event.from;
    int16_t px = pressed.x;
    int16_t py = pressed.y;
    info("%u background pressed x=%d y=%d, free RAM %u/%u free PSRAM %u/%u", currentTime, px, py, freeHeap, totalHeap,
         freePSRAM, totalPSRAM);
    switch (gScreenMode) {
      case ScreenMode::kOff: {
        gLastScreenInteractionTime = currentTime;
        setCore2ScreenBrightness(gOnBrightness);
#if BUTTON_LOCK
        info("%u starting unlock sequence from button press", currentTime);
        gScreenMode = ScreenMode::kLocked1;
        unlock1Button.draw();
#else   // BUTTON_LOCK
        info("%u unlocking from button press", currentTime);
        startMainMenu(player, currentTime);
        gLastScreenInteractionTime = currentTime;
#endif  // BUTTON_LOCK
      } break;
      case ScreenMode::kMainMenu: {
        gScreenMode = ScreenMode::kFullScreenPattern;
        if (px < 160 && py < 120) {
          info("%u pattern screen pressed", currentTime);
          hideMainMenuButtons();
          M5.Lcd.fillScreen(BLACK);
          core2ScreenRenderer.setFullScreen(true);
          gLastScreenInteractionTime = currentTime;
        }
      } break;
      case ScreenMode::kFullScreenPattern: {
        info("%u full screen pattern pressed", currentTime);
        core2ScreenRenderer.setFullScreen(false);
        startMainMenu(player, currentTime);
        gLastScreenInteractionTime = currentTime;
      } break;
      case ScreenMode::kPatternControlMenu: {
      } break;
      case ScreenMode::kSystemMenu: {
      } break;
      case ScreenMode::kLocked1:  // fallthrough.
      case ScreenMode::kLocked2: {
        info("%u locking screen due to background press while unlocking");
        lockScreen(currentTime);
      } break;
    }
  }
  if (nextButton.wasPressed()) {
    info("%u next pressed", currentTime);
    if (gScreenMode == ScreenMode::kMainMenu) {
      gLastScreenInteractionTime = currentTime;
      player.next(currentTime);
    }
  }
  if (loopButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kMainMenu) {
      info("%u loop pressed", currentTime);
      gLastScreenInteractionTime = currentTime;
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
    } else {
      info("%u ignoring loop pressed", currentTime);
    }
  }
  if (patternControlButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kMainMenu) {
      info("%u pattern control button pressed", currentTime);
      patternControlButtonPressed(player, currentTime);
    } else {
      info("%u ignoring pattern control button pressed", currentTime);
    }
  }
  if (systemButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kMainMenu) {
      info("%u system button pressed", currentTime);
      gScreenMode = ScreenMode::kSystemMenu;
      gLastScreenInteractionTime = currentTime;
      hideMainMenuButtons();
      core2ScreenRenderer.setEnabled(false);
      M5.Lcd.fillScreen(BLACK);
      drawSystemMenuButtons();
      drawSystemTextLines(player, currentTime);
    } else {
      info("%u ignoring system button pressed", currentTime);
    }
  }
  if (backButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kSystemMenu ||
        gScreenMode == ScreenMode::kPatternControlMenu && gPatternControlMenu.backPressed()) {
      info("%u back button pressed", currentTime);
      gLastScreenInteractionTime = currentTime;
      hidePatternControlMenuButtons();
      hideSystemMenuButtons();
      startMainMenu(player, currentTime);
    } else {
      info("%u ignoring back button pressed", currentTime);
    }
  }
  if (downButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kPatternControlMenu) {
      info("%u down button pressed", currentTime);
      gLastScreenInteractionTime = currentTime;
      gPatternControlMenu.downPressed();
    } else {
      info("%u ignoring down button pressed", currentTime);
    }
  }
  if (upButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kPatternControlMenu) {
      info("%u up button pressed", currentTime);
      gLastScreenInteractionTime = currentTime;
      gPatternControlMenu.upPressed();
    } else {
      info("%u ignoring up button pressed", currentTime);
    }
  }
  if (overrideButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kPatternControlMenu) {
      info("%u override button pressed", currentTime);
      gLastScreenInteractionTime = currentTime;
      gPatternControlMenu.overridePressed(player, currentTime);
    } else {
      info("%u ignoring override button pressed", currentTime);
    }
  }
  if (confirmButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kPatternControlMenu) {
      info("%u confirm button pressed", currentTime);
      confirmButtonPressed(player, currentTime);
    } else {
      info("%u ignoring confirm button pressed", currentTime);
    }
  }
  if (lockButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kSystemMenu) {
      info("%u lock button pressed", currentTime);
      lockScreen(currentTime);
    } else {
      info("%u ignoring lock button pressed", currentTime);
    }
  }
  if (shutdownButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kSystemMenu) {
      info("%u shutdown button pressed", currentTime);
      M5.shutdown();
    } else {
      info("%u ignoring shutdown button pressed", currentTime);
    }
  }
  if (unlock2Button.wasPressed()) {
    gLastScreenInteractionTime = currentTime;
    if (gScreenMode == ScreenMode::kLocked2) {
      info("%u unlock2 button pressed", currentTime);
      unlock2Button.hide();
      startMainMenu(player, currentTime);
    } else if (gScreenMode == ScreenMode::kMainMenu) {
      info("%u unlock2 button unexpectedly pressed in main menu, treating as pattern control button", currentTime);
      patternControlButtonPressed(player, currentTime);
    } else if (gScreenMode == ScreenMode::kPatternControlMenu) {
      info("%u unlock2 button unexpectedly pressed in pattern control menu, treating as confirm button", currentTime);
      confirmButtonPressed(player, currentTime);
    } else if (gScreenMode == ScreenMode::kSystemMenu) {
      info("%u unlock2 button unexpectedly pressed in system menu, treating as shutdown button", currentTime);
      M5.shutdown();
    } else {
      info("%u ignoring unlock2 button pressed", currentTime);
    }
  }
  if (unlock1Button.wasPressed()) {
    gLastScreenInteractionTime = currentTime;
    if (gScreenMode == ScreenMode::kLocked1) {
      info("%u unlock1 button pressed", currentTime);
      gScreenMode = ScreenMode::kLocked2;
      unlock1Button.hide();
      M5.Lcd.fillScreen(BLACK);
      unlock2Button.draw();
    } else if (gScreenMode == ScreenMode::kMainMenu) {
      info("%u unlock1 button unexpectedly pressed in main menu, treating as next button", currentTime);
      player.next(currentTime);
    } else {
      info("%u ignoring unlock1 button pressed", currentTime);
    }
  }
  if (ledPlusButton.wasPressed()) {
    info("%u ledPlusButton button pressed", currentTime);
    if (gLedBrightness < 255 && gScreenMode == ScreenMode::kSystemMenu) {
      gLedBrightness++;
      info("%u setting LED brightness to %u", currentTime, gLedBrightness);
      drawSystemTextLines(player, currentTime);
    }
  }
  if (ledMinusButton.wasPressed()) {
    info("%u ledMinusButton button pressed", currentTime);
    if (gLedBrightness > 0 && gScreenMode == ScreenMode::kSystemMenu) {
      gLedBrightness--;
      info("%u setting LED brightness to %u", currentTime, gLedBrightness);
      drawSystemTextLines(player, currentTime);
    }
  }
  if (screenPlusButton.wasPressed()) {
    info("%u screenPlusButton button pressed", currentTime);
    if (gOnBrightness < kMaxOnBrightness && gScreenMode == ScreenMode::kSystemMenu) {
      gOnBrightness++;
      setCore2ScreenBrightness(gOnBrightness);
      drawSystemTextLines(player, currentTime);
    }
  }
  if (screenMinusButton.wasPressed()) {
    info("%u screenMinusButton button pressed", currentTime);
    if (gOnBrightness > kMinOnBrightness && gScreenMode == ScreenMode::kSystemMenu) {
      gOnBrightness--;
      setCore2ScreenBrightness(gOnBrightness);
      drawSystemTextLines(player, currentTime);
    }
  }

  std::string patternName = player.currentEffectName();
  if (patternName != gCurrentPatternName) {
    gCurrentPatternName = patternName;
    if (gScreenMode == ScreenMode::kMainMenu) {
      info("%u drawing new pattern name in pattern control button", currentTime);
      patternControlButton.draw();
    }
  }
#if BUTTON_LOCK
  if (gLastScreenInteractionTime >= 0) {
    Milliseconds lockTime = kLockDelay;
    if (gScreenMode == ScreenMode::kLocked1 || gScreenMode == ScreenMode::kLocked2) { lockTime = kUnlockingTime; }
    if (currentTime - gLastScreenInteractionTime > lockTime) {
      info("%u Locking screen due to inactivity", currentTime);
      lockScreen(currentTime);
    }
  }
#endif  // BUTTON_LOCK
}

}  // namespace jazzlights

#endif  // CORE2AWS && WEARABLE
