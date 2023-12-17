#include "jazzlights/ui_core2.h"

#include "jazzlights/config.h"

#if JL_IS_CONTROLLER(CORE2AWS)

#include <M5Core2.h>

#include "jazzlights/layouts/matrix.h"
#include "jazzlights/networks/arduino_esp_wifi.h"
#include "jazzlights/networks/esp32_ble.h"

namespace jazzlights {

#if CORE2AWS_LCD_ENABLED

namespace {

static constexpr uint8_t kDefaultOnBrightness = 12;
static constexpr uint8_t kMinOnBrightness = 1;
static constexpr uint8_t kMaxOnBrightness = 20;

#if JL_DEV
static constexpr uint8_t kInitialLedBrightness = 2;
#else   // JL_DEV
static constexpr uint8_t kInitialLedBrightness = 32;
#endif  // JL_DEV

enum class ScreenMode {
  kOff,
  kLocked1,
  kLocked2,
  kMainMenu,
  kFullScreenPattern,
  kPatternControlMenu,
  kSystemMenu,
};
#if JL_BUTTON_LOCK
static constexpr ScreenMode kInitialScreenMode = ScreenMode::kOff;
#else   // JL_BUTTON_LOCK
static constexpr ScreenMode kInitialScreenMode = ScreenMode::kMainMenu;
#endif  // JL_BUTTON_LOCK

static void SetCore2ScreenBrightness(uint8_t brightness, bool allowUnsafe = false) {
  jll_info("Setting screen brightness to %u%s", brightness, (allowUnsafe ? " (unsafe enabled)" : ""));
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

static void SetupButtonsDrawZone() {
  constexpr int16_t kButtonDrawOffset = 3;
  for (Button* b : Button::instances) {
    b->drawZone = ::Zone(/*x=*/b->x + kButtonDrawOffset,
                         /*y=*/b->y + kButtonDrawOffset,
                         /*w=*/b->w - 2 * kButtonDrawOffset,
                         /*h=*/b->h - 2 * kButtonDrawOffset);
  }
}

void SetDefaultPrecedence(Player& player, Milliseconds currentTime) {
  player.updatePrecedence(4000, 1000, currentTime);
}

void SetOverridePrecedence(Player& player, Milliseconds currentTime) {
  player.updatePrecedence(36000, 5000, currentTime);
}

void DrawSystemTextLine(uint8_t i, const char* text) {
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

static const Matrix kCore2ScreenPixels(40, 30);

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

}  // namespace

ScreenMode gScreenMode = kInitialScreenMode;
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

class PatternControlMenu {
 public:
  void draw() {
    jll_info("PatternControlMenu::draw()");
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
      SetOverridePrecedence(player, currentTime);
    } else {
      SetDefaultPrecedence(player, currentTime);
    }
  }
  bool confirmPressed(Player& player, Milliseconds currentTime) {
    const char* confirmLabel;
    if (state_ == State::kPattern) {
      State nextState = kSelectablePatterns[selectedPatternIndex_].nextState;
      if (nextState == State::kPalette || nextState == State::kColor) {
        jll_info("%u Pattern %s confirmed now asking for %s", currentTime,
                 kSelectablePatterns[selectedPatternIndex_].name, (nextState == State::kPalette ? "palette" : "color"));
        state_ = nextState;
        draw();
      } else {
        jll_info("%u Pattern %s confirmed now playing", currentTime, kSelectablePatterns[selectedPatternIndex_].name);
        player.stopForcePalette(currentTime);
        return setPattern(player, kSelectablePatterns[selectedPatternIndex_].bits, currentTime);
      }
    } else if (state_ == State::kPalette) {
      jll_info("%u Pattern %s and palette %s confirmed now playing", currentTime,
               kSelectablePatterns[selectedPatternIndex_].name, kPaletteNames[selectedPaletteIndex_]);
      return setPatternWithPalette(player, kSelectablePatterns[selectedPatternIndex_].bits, selectedPaletteIndex_,
                                   currentTime);
    } else if (state_ == State::kColor) {
      jll_info("%u Pattern %s and color %s confirmed now playing", currentTime,
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
    jll_info("drawing confirm button with label %s", confirmLabel);
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
  jll_info("drawing pattern control button bg 0x%x outline 0x%x text 0x%x", bc.bg, bc.outline, bc.text);
  // First call default draw function to draw button text, outline, and background.
  M5Buttons::drawFunction(b, bc);
  // Then add custom pattern string.
  M5.Lcd.setTextColor(bc.text, bc.bg);
  M5.Lcd.setTextDatum(BC_DATUM);  // Bottom Center.
  M5.Lcd.drawString(gCurrentPatternName.c_str(), /*x=*/80, /*y=*/210);
}

void drawSystemButton(Button& b, ButtonColors bc) {
  if (gScreenMode != ScreenMode::kMainMenu) { return; }
  jll_info("drawing system button bg 0x%x outline 0x%x text 0x%x", bc.bg, bc.outline, bc.text);
  // First call default draw function to draw button text, outline, and background.
  M5Buttons::drawFunction(b, bc);
  // Then add custom system info.
  M5.Lcd.setTextColor(bc.text, bc.bg);
  M5.Lcd.setTextDatum(BC_DATUM);  // Bottom Center.
  M5.Lcd.drawString(BOOT_MESSAGE, /*x=*/240, /*y=*/210);
}

void DrawMainMenuButtons() {
  jll_info("DrawMainMenuButtons");
  unlock1Button.hide();
  unlock2Button.hide();
  nextButton.draw();
  loopButton.draw();
  patternControlButton.draw();
  systemButton.draw();
}

void HideMainMenuButtons() {
  jll_info("HideMainMenuButtons");
  nextButton.hide();
  loopButton.hide();
  patternControlButton.hide();
  systemButton.hide();
}

void DrawPatternControlMenuButtons() {
  jll_info("DrawPatternControlMenuButtons");
  unlock1Button.hide();
  unlock2Button.hide();
  backButton.draw();
  downButton.draw();
  upButton.draw();
  overrideButton.draw();
  confirmButton.draw();
}

void HidePatternControlMenuButtons() {
  jll_info("HidePatternControlMenuButtons");
  backButton.hide();
  downButton.hide();
  upButton.hide();
  overrideButton.hide();
  confirmButton.hide();
}

void DrawSystemMenuButtons() {
  jll_info("DrawSystemMenuButtons");
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

void HideSystemMenuButtons() {
  jll_info("HideSystemMenuButtons");
  backButton.hide();
  lockButton.hide();
  shutdownButton.hide();
  ledPlusButton.hide();
  ledMinusButton.hide();
  screenPlusButton.hide();
  screenMinusButton.hide();
}

void startMainMenu(Player& player, Milliseconds currentTime) {
  gScreenMode = ScreenMode::kMainMenu;
  M5.Lcd.fillScreen(BLACK);
  DrawMainMenuButtons();
  core2ScreenRenderer.setEnabled(true);
}

void lockScreen(Milliseconds currentTime) {
  gLastScreenInteractionTime = -1;
  gScreenMode = ScreenMode::kOff;
  HideSystemMenuButtons();
  HideMainMenuButtons();
  HidePatternControlMenuButtons();
  core2ScreenRenderer.setEnabled(false);
  SetCore2ScreenBrightness(0);
  M5.Lcd.fillScreen(BLACK);
}

void patternControlButtonPressed(Player& player, Milliseconds currentTime) {
  gScreenMode = ScreenMode::kPatternControlMenu;
  gLastScreenInteractionTime = currentTime;
  HideMainMenuButtons();
  core2ScreenRenderer.setEnabled(false);
  M5.Lcd.fillScreen(BLACK);
  DrawPatternControlMenuButtons();
  gPatternControlMenu.draw();
}

void confirmButtonPressed(Player& player, Milliseconds currentTime) {
  gLastScreenInteractionTime = currentTime;
  if (gPatternControlMenu.confirmPressed(player, currentTime)) {
    HidePatternControlMenuButtons();
    startMainMenu(player, currentTime);
  }
}

Core2AwsUi::Core2AwsUi(Player& player, Milliseconds currentTime)
    : ArduinoUi(player, currentTime), ledBrightness_(kInitialLedBrightness), onBrightness_(kDefaultOnBrightness) {}

void Core2AwsUi::InitialSetup(Milliseconds currentTime) {
  player_.SetBrightness(ledBrightness_);
  M5.begin(/*LCDEnable=*/true, /*SDEnable=*/false,
           /*SerialEnable=*/false, /*I2CEnable=*/false,
           /*mode=*/kMBusModeOutput);
  if (gScreenMode == ScreenMode::kOff) {
    SetCore2ScreenBrightness(0);
  } else {
    SetCore2ScreenBrightness(onBrightness_);
  }
  // TODO switch mode to kMBusModeInput once we power from pins instead of USB.
  M5.Lcd.fillScreen(BLACK);
  HidePatternControlMenuButtons();
  HideSystemMenuButtons();
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
  SetupButtonsDrawZone();
  player_.addStrand(kCore2ScreenPixels, core2ScreenRenderer);
  SetDefaultPrecedence(player_, currentTime);
}

void Core2AwsUi::FinalSetup(Milliseconds currentTime) {
  gCurrentPatternName = player_.currentEffectName();
  if (gScreenMode == ScreenMode::kMainMenu) {
    startMainMenu(player_, currentTime);
  } else {
    HideMainMenuButtons();
    core2ScreenRenderer.setEnabled(false);
  }
}

void Core2AwsUi::DrawSystemTextLines(Milliseconds currentTime) {
  size_t i = 0;
  char line[100] = {};
  // LED Brighness.
  snprintf(line, sizeof(line) - 1, "LED Brgt %u/255", ledBrightness_);
  DrawSystemTextLine(i++, line);
  // Screen Brightness.
  snprintf(line, sizeof(line) - 1, "Screen Brgt %u/20", onBrightness_);
  DrawSystemTextLine(i++, line);
  // BLE.
  snprintf(line, sizeof(line) - 1, "BLE: %s", Esp32BleNetwork::get()->getStatusStr(currentTime).c_str());
  DrawSystemTextLine(i++, line);
  // Wi-Fi.
  snprintf(line, sizeof(line) - 1, "Wi-Fi: %s", ArduinoEspWiFiNetwork::get()->getStatusStr(currentTime).c_str());
  DrawSystemTextLine(i++, line);
  // Other.
  const Network* followedNextHopNetwork = player_.followedNextHopNetwork();
  if (followedNextHopNetwork == nullptr) {
    snprintf(line, sizeof(line) - 1, "Leading");
  } else {
    snprintf(line, sizeof(line) - 1, "Following %s nh=%u", followedNextHopNetwork->shortNetworkName(),
             player_.currentNumHops());
  }
  DrawSystemTextLine(i++, line);
}

void Core2AwsUi::RunLoop(Milliseconds currentTime) {
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
    jll_info("%u background pressed x=%d y=%d, free RAM %u/%u free PSRAM %u/%u", currentTime, px, py, freeHeap,
             totalHeap, freePSRAM, totalPSRAM);
    switch (gScreenMode) {
      case ScreenMode::kOff: {
        gLastScreenInteractionTime = currentTime;
        SetCore2ScreenBrightness(onBrightness_);
#if JL_BUTTON_LOCK
        jll_info("%u starting unlock sequence from button press", currentTime);
        gScreenMode = ScreenMode::kLocked1;
        unlock1Button.draw();
#else   // JL_BUTTON_LOCK
        jll_info("%u unlocking from button press", currentTime);
        startMainMenu(player_, currentTime);
        gLastScreenInteractionTime = currentTime;
#endif  // JL_BUTTON_LOCK
      } break;
      case ScreenMode::kMainMenu: {
        gScreenMode = ScreenMode::kFullScreenPattern;
        if (px < 160 && py < 120) {
          jll_info("%u pattern screen pressed", currentTime);
          HideMainMenuButtons();
          M5.Lcd.fillScreen(BLACK);
          core2ScreenRenderer.setFullScreen(true);
          gLastScreenInteractionTime = currentTime;
        }
      } break;
      case ScreenMode::kFullScreenPattern: {
        jll_info("%u full screen pattern pressed", currentTime);
        core2ScreenRenderer.setFullScreen(false);
        startMainMenu(player_, currentTime);
        gLastScreenInteractionTime = currentTime;
      } break;
      case ScreenMode::kPatternControlMenu: {
      } break;
      case ScreenMode::kSystemMenu: {
      } break;
      case ScreenMode::kLocked1:  // fallthrough.
      case ScreenMode::kLocked2: {
        jll_info("%u locking screen due to background press while unlocking");
        lockScreen(currentTime);
      } break;
    }
  }
  if (nextButton.wasPressed()) {
    jll_info("%u next pressed", currentTime);
    if (gScreenMode == ScreenMode::kMainMenu) {
      gLastScreenInteractionTime = currentTime;
      player_.next(currentTime);
    }
  }
  if (loopButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kMainMenu) {
      jll_info("%u loop pressed", currentTime);
      gLastScreenInteractionTime = currentTime;
      if (player_.isLooping()) {
        player_.stopLooping(currentTime);
        loopButton.setLabel("Loop");
        loopButton.off = idleCol;
      } else {
        player_.loopOne(currentTime);
        loopButton.setLabel("Looping");
        loopButton.off = idleEnabledCol;
      }
      loopButton.draw();
    } else {
      jll_info("%u ignoring loop pressed", currentTime);
    }
  }
  if (patternControlButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kMainMenu) {
      jll_info("%u pattern control button pressed", currentTime);
      patternControlButtonPressed(player_, currentTime);
    } else {
      jll_info("%u ignoring pattern control button pressed", currentTime);
    }
  }
  if (systemButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kMainMenu) {
      jll_info("%u system button pressed", currentTime);
      gScreenMode = ScreenMode::kSystemMenu;
      gLastScreenInteractionTime = currentTime;
      HideMainMenuButtons();
      core2ScreenRenderer.setEnabled(false);
      M5.Lcd.fillScreen(BLACK);
      DrawSystemMenuButtons();
      DrawSystemTextLines(currentTime);
    } else {
      jll_info("%u ignoring system button pressed", currentTime);
    }
  }
  if (backButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kSystemMenu ||
        gScreenMode == ScreenMode::kPatternControlMenu && gPatternControlMenu.backPressed()) {
      jll_info("%u back button pressed", currentTime);
      gLastScreenInteractionTime = currentTime;
      HidePatternControlMenuButtons();
      HideSystemMenuButtons();
      startMainMenu(player_, currentTime);
    } else {
      jll_info("%u ignoring back button pressed", currentTime);
    }
  }
  if (downButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kPatternControlMenu) {
      jll_info("%u down button pressed", currentTime);
      gLastScreenInteractionTime = currentTime;
      gPatternControlMenu.downPressed();
    } else {
      jll_info("%u ignoring down button pressed", currentTime);
    }
  }
  if (upButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kPatternControlMenu) {
      jll_info("%u up button pressed", currentTime);
      gLastScreenInteractionTime = currentTime;
      gPatternControlMenu.upPressed();
    } else {
      jll_info("%u ignoring up button pressed", currentTime);
    }
  }
  if (overrideButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kPatternControlMenu) {
      jll_info("%u override button pressed", currentTime);
      gLastScreenInteractionTime = currentTime;
      gPatternControlMenu.overridePressed(player_, currentTime);
    } else {
      jll_info("%u ignoring override button pressed", currentTime);
    }
  }
  if (confirmButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kPatternControlMenu) {
      jll_info("%u confirm button pressed", currentTime);
      confirmButtonPressed(player_, currentTime);
    } else {
      jll_info("%u ignoring confirm button pressed", currentTime);
    }
  }
  if (lockButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kSystemMenu) {
      jll_info("%u lock button pressed", currentTime);
      lockScreen(currentTime);
    } else {
      jll_info("%u ignoring lock button pressed", currentTime);
    }
  }
  if (shutdownButton.wasPressed()) {
    if (gScreenMode == ScreenMode::kSystemMenu) {
      jll_info("%u shutdown button pressed", currentTime);
      M5.shutdown();
    } else {
      jll_info("%u ignoring shutdown button pressed", currentTime);
    }
  }
  if (unlock2Button.wasPressed()) {
    gLastScreenInteractionTime = currentTime;
    if (gScreenMode == ScreenMode::kLocked2) {
      jll_info("%u unlock2 button pressed", currentTime);
      unlock2Button.hide();
      startMainMenu(player_, currentTime);
    } else if (gScreenMode == ScreenMode::kMainMenu) {
      jll_info("%u unlock2 button unexpectedly pressed in main menu, treating as pattern control button", currentTime);
      patternControlButtonPressed(player_, currentTime);
    } else if (gScreenMode == ScreenMode::kPatternControlMenu) {
      jll_info("%u unlock2 button unexpectedly pressed in pattern control menu, treating as confirm button",
               currentTime);
      confirmButtonPressed(player_, currentTime);
    } else if (gScreenMode == ScreenMode::kSystemMenu) {
      jll_info("%u unlock2 button unexpectedly pressed in system menu, treating as shutdown button", currentTime);
      M5.shutdown();
    } else {
      jll_info("%u ignoring unlock2 button pressed", currentTime);
    }
  }
  if (unlock1Button.wasPressed()) {
    gLastScreenInteractionTime = currentTime;
    if (gScreenMode == ScreenMode::kLocked1) {
      jll_info("%u unlock1 button pressed", currentTime);
      gScreenMode = ScreenMode::kLocked2;
      unlock1Button.hide();
      M5.Lcd.fillScreen(BLACK);
      unlock2Button.draw();
    } else if (gScreenMode == ScreenMode::kMainMenu) {
      jll_info("%u unlock1 button unexpectedly pressed in main menu, treating as next button", currentTime);
      player_.next(currentTime);
    } else {
      jll_info("%u ignoring unlock1 button pressed", currentTime);
    }
  }
  if (ledPlusButton.wasPressed()) {
    jll_info("%u ledPlusButton button pressed", currentTime);
    if (ledBrightness_ < 255 && gScreenMode == ScreenMode::kSystemMenu) {
      ledBrightness_++;
      jll_info("%u setting LED brightness to %u", currentTime, ledBrightness_);
      player_.SetBrightness(ledBrightness_);
      DrawSystemTextLines(currentTime);
    }
  }
  if (ledMinusButton.wasPressed()) {
    jll_info("%u ledMinusButton button pressed", currentTime);
    if (ledBrightness_ > 0 && gScreenMode == ScreenMode::kSystemMenu) {
      ledBrightness_--;
      jll_info("%u setting LED brightness to %u", currentTime, ledBrightness_);
      player_.SetBrightness(ledBrightness_);
      DrawSystemTextLines(currentTime);
    }
  }
  if (screenPlusButton.wasPressed()) {
    jll_info("%u screenPlusButton button pressed", currentTime);
    if (onBrightness_ < kMaxOnBrightness && gScreenMode == ScreenMode::kSystemMenu) {
      onBrightness_++;
      SetCore2ScreenBrightness(onBrightness_);
      DrawSystemTextLines(currentTime);
    }
  }
  if (screenMinusButton.wasPressed()) {
    jll_info("%u screenMinusButton button pressed", currentTime);
    if (onBrightness_ > kMinOnBrightness && gScreenMode == ScreenMode::kSystemMenu) {
      onBrightness_--;
      SetCore2ScreenBrightness(onBrightness_);
      DrawSystemTextLines(currentTime);
    }
  }

  std::string patternName = player_.currentEffectName();
  if (patternName != gCurrentPatternName) {
    gCurrentPatternName = patternName;
    if (gScreenMode == ScreenMode::kMainMenu) {
      jll_info("%u drawing new pattern name in pattern control button", currentTime);
      patternControlButton.draw();
    }
  }
#if JL_BUTTON_LOCK
  if (gLastScreenInteractionTime >= 0) {
    Milliseconds lockTime = kLockDelay;
    if (gScreenMode == ScreenMode::kLocked1 || gScreenMode == ScreenMode::kLocked2) { lockTime = kUnlockingTime; }
    if (currentTime - gLastScreenInteractionTime > lockTime) {
      jll_info("%u Locking screen due to inactivity", currentTime);
      lockScreen(currentTime);
    }
  }
#endif  // JL_BUTTON_LOCK
}

#endif  // CORE2AWS_LCD_ENABLED

}  // namespace jazzlights

#endif  // CORE2AWS

/*
  GPIO pins on Core2AWS for vehicles
   0 = NS4168-LRCK = PortE1c
   1 = USB_CP2104 RXD = PortD1c
   2 = NS4168-DATA = PortE1b = PortE2c
   3 = USB_CP2104 TXD = PortD2c
   4 = TF CS
   5 = LCD CS
  13 = PortC1
  14 = PortC2
  15 = LCD DC
  18 = W5500 SCLK = LCD SCLK = TF SCLK
  19 = W5500 RST = PortE2a
  21 = internal I2C SDA = PortD2b
  22 = internal I2C SCL = PortD1b
  23 = W5500 MOSI = LCD MOSI = TF MOSI
  25 = LED (only when AWS expansion is used) = PortE2b
  26 = W5500 CS = PortB2
  27 = PortE1a
  32 = PortA1
  33 = PortA2
  34 = W5500 INTn = PortD1a
  35 = PortD2a
  36 = PortB1
  38 = LCD MISO = TF MISO = W5500 MISO

  Good: A1,A2,B1,C1,C2,D2a,E1a,E2b -- total of 8 pins
  Taken by W5500 ethernet: B2,D1

  https://docs.m5stack.com/en/core/core2_for_aws
  https://docs.m5stack.com/en/base/lan_v12

  Note that the pinout docs for the ethernet Core2 base are wrong:
  bus position | Core2 pin | W5500 module pin
             9 |        38 | 19 = W5500 MISO
            15 |        13 | 16
            16 |        14 | 17
            19 |        32 |  2
            20 |        33 |  5
            21 |        27 | 12
            22 |        19 | 13 = W5500 RST
            23 |         2 | 15
*/
