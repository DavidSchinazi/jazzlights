#include "jazzlights/ui/ui_core2.h"

#include "jazzlights/config.h"

#if JL_IS_CONTROLLER(CORE2AWS)

#include <M5Unified.h>

#include "jazzlights/layout/matrix.h"
#include "jazzlights/network/esp32_ble.h"
#include "jazzlights/network/wifi.h"

namespace jazzlights {

#if CORE2AWS_LCD_ENABLED

namespace {

static constexpr uint8_t kDefaultOnBrightness = 32;
static constexpr uint8_t kMinOnBrightness = 1;
static constexpr uint8_t kMaxOnBrightness = 100;

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

void CorePowerOff() {
  // TODO reenable once we figure out IRAM limitation.
  // M5.Power.powerOff();
}

static void SetCore2ScreenBrightness(uint8_t brightness) {
  jll_info("Setting screen brightness to %u", brightness);
  if (brightness == 0) {
    M5.Display.clearDisplay();
    M5.Display.sleep();
    return;
  }
  M5.Display.setBrightness(brightness);
  M5.Display.wakeup();
}

void SetDefaultPrecedence(Player& player, Milliseconds currentTime) {
  player.updatePrecedence(4000, 1000, currentTime);
}

void SetOverridePrecedence(Player& player, Milliseconds currentTime) {
  player.updatePrecedence(OverridePrecedence(), 5000, currentTime);
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
  void setEnabled(bool enabled) { enabled_ = enabled; }
  void renderPixel(size_t index, CRGB color) override {
    if (!enabled_) { return; }
    uint16_t color16 =
        ((uint16_t)(color.red & 0xF8) << 8) | ((uint16_t)(color.green & 0xFC) << 3) | ((color.blue & 0xF8) >> 3);
    int32_t x = index % 40;
    int32_t y = index / 40;
    int32_t factor = fullScreen_ ? 8 : 4;
    for (size_t xi = 0; xi < factor; xi++) {
      for (size_t yi = 0; yi < factor; yi++) { rowColors16_[x * factor + xi + yi * 40 * factor] = color16; }
    }
    if (x == 39) {
      bool swap = M5.Lcd.getSwapBytes();
      M5.Lcd.setSwapBytes(true);
      M5.Lcd.pushImage(/*x0=*/0, /*y0=*/y * factor, /*w=*/40 * factor, /*h=*/factor, rowColors16_);
      M5.Lcd.setSwapBytes(swap);
    }
  }

 private:
  bool enabled_ = true;
  bool fullScreen_ = false;
  // We only process 8 rows in memory at a time because the entire image does not fit on the stack or in SRAM.
  // One potential alternative would be to keep the image in PSRAM but that would not necessarily be faster.
  uint16_t rowColors16_[320 * 8] = {};
};

}  // namespace

ScreenMode gScreenMode = kInitialScreenMode;
Core2ScreenRenderer core2ScreenRenderer;

class TouchButton {
 public:
  using PaintFunction = std::function<void(TouchButton* button, int outline, int fill, int textColor)>;
  void Setup(int16_t x, int16_t y, uint16_t w, uint16_t h, const char* label) {
    x_ = x;
    y_ = y;
    w_ = w;
    h_ = h;
    lgfxButton_.initButtonUL(&M5.Lcd, x, y, w, h,
                             /*outline=*/TFT_WHITE, /*fill=*/TFT_BLACK, /*textcolor=*/TFT_WHITE, "");
    SetLabelText(label);
  }

  void SetLabelText(const char* label) {
    label_ = std::string(label);
    Paint();
  }

  void SetLabelDatum(int16_t x_delta, int16_t y_delta,
                     lgfx::textdatum::textdatum_t datum = lgfx::textdatum::textdatum_t::middle_center) {
    lgfxButton_.setLabelDatum(x_delta, y_delta, datum);
    dx_ = x_delta;
    dy_ = y_delta;
    Paint();
  }

  void Hide() {
    hidden_ = true;
    MaybePaint();
  }

  void Draw(bool force = false) {
    hidden_ = false;
    MaybePaint(force);
  }

  void Paint();

  bool IsPressed() const {
    if (hidden_) { return false; }
    return lgfxButton_.isPressed();
  }
  bool JustPressed() const {
    if (hidden_) { return false; }
    return lgfxButton_.justPressed();
  }
  bool JustReleased() const {
    if (hidden_) { return false; }
    return lgfxButton_.justReleased();
  }

  bool HandlePress(int16_t x, int16_t y) {
    bool ret = false;
    if (!hidden_) {
      ret = lgfxButton_.contains(x, y);
      lgfxButton_.press(ret);
    }
    MaybePaint();
    return ret;
  }
  void HandleIdle() {
    lgfxButton_.press(false);
    MaybePaint();
  }
  void MaybePaint(bool force = false) {
    DrawState drawState;
    if (hidden_) {
      drawState = DrawState::kHidden;
    } else if (IsPressed()) {
      drawState = DrawState::kPressed;
    } else {
      drawState = DrawState::kIdle;
    }
    if (force || drawState != lastDrawState_) {
      lastDrawState_ = drawState;
      Paint();
    }
  }

  void PaintRectangle(int fill, int outline) {
    uint16_t cornerRadius = std::min<uint16_t>(w_, h_) / 4;
    M5.Lcd.fillRoundRect(x_ + 1, y_ + 1, w_ - 2, h_ - 2, cornerRadius, fill);
    M5.Lcd.drawRoundRect(x_ + 1, y_ + 1, w_ - 2, h_ - 2, cornerRadius, outline);
  }
  void PaintText(int textColor, int fill, const char* label = nullptr) {
    const char* printLabel = (label != nullptr ? label : label_.c_str());
    M5.Lcd.setTextColor(textColor, fill);
    M5.Lcd.drawString(printLabel, x_ + (w_ / 2) + dx_, y_ + (h_ / 2) + dy_);
  }

  bool hidden() const { return hidden_; }
  void SetHighlight(bool highlight) { highlight_ = highlight; }
  void SetCustomPaintFunction(PaintFunction customPaintFunction) { customPaintFunction_ = customPaintFunction; }

 private:
  enum class DrawState {
    kNotDrawn,
    kHidden,
    kIdle,
    kPressed,
  };
  LGFX_Button lgfxButton_;
  std::string label_;
  bool hidden_ = true;
  bool highlight_ = false;
  int16_t x_ = 0;
  int16_t y_ = 0;
  uint16_t w_ = 0;
  uint16_t h_ = 0;
  int16_t dx_ = 0;
  int16_t dy_ = 0;
  DrawState lastDrawState_ = DrawState::kNotDrawn;
  PaintFunction customPaintFunction_;
};

class TouchButtonManager {
 public:
  TouchButton* AddButton(int16_t x, int16_t y, uint16_t w, uint16_t h, const char* label) {
    buttons_.push_back(std::unique_ptr<TouchButton>(new TouchButton()));
    std::unique_ptr<TouchButton>& button = buttons_.back();
    button->Setup(x, y, w, h, label);
    return button.get();
  }

  static TouchButtonManager* Get() {
    static TouchButtonManager sTouchButtonManager;
    return &sTouchButtonManager;
  }

  bool HandlePress(int16_t x, int16_t y) {
    bool ret = false;
    for (auto& button : buttons_) {
      if (button->hidden()) {
        if (button->HandlePress(x, y)) { ret = true; }
      }
    }
    for (auto& button : buttons_) {
      if (!button->hidden()) {
        if (button->HandlePress(x, y)) { ret = true; }
      }
    }
    return ret;
  }
  void HandleIdle() {
    for (auto& button : buttons_) {
      if (button->hidden()) { button->HandleIdle(); }
    }
    for (auto& button : buttons_) {
      if (!button->hidden()) { button->HandleIdle(); }
    }
  }
  void MaybePaint() {
    for (auto& button : buttons_) {
      if (button->hidden()) { button->MaybePaint(); }
    }
    for (auto& button : buttons_) {
      if (!button->hidden()) { button->MaybePaint(); }
    }
  }

  void RedrawRightHalf() {
    M5.Lcd.fillRect(/*x=*/165, /*y=*/0, /*w=*/155, /*h=*/240, BLACK);
    for (auto& button : buttons_) {
      if (button->hidden()) { button->Paint(); }
    }
    for (auto& button : buttons_) {
      if (!button->hidden()) { button->Paint(); }
    }
  }

  void Redraw() {
    M5.Lcd.fillScreen(BLACK);
    for (auto& button : buttons_) {
      if (button->hidden()) { button->Paint(); }
    }
    for (auto& button : buttons_) {
      if (!button->hidden()) { button->Paint(); }
    }
  }

 private:
  std::vector<std::unique_ptr<TouchButton>> buttons_;
};

void TouchButton::Paint() {
  TouchButtonManager::Get()->MaybePaint();
  if (hidden_) {
    M5.Lcd.drawRect(x_, y_, w_, h_, TFT_BLACK);
    return;
  }

  auto previousTextStyle = M5.Lcd.getTextStyle();

  M5.Lcd.setTextSize(1.0f, 0.0f);
  M5.Lcd.setFont(&FreeSans9pt7b);
  M5.Lcd.setTextDatum(lgfx::textdatum::textdatum_t::middle_center);
  M5.Lcd.setTextPadding(0);

  int fill = TFT_BLACK;
  int textColor = TFT_WHITE;
  int outline = TFT_WHITE;
  if (IsPressed()) {
    fill = TFT_RED;
  } else if (highlight_) {
    fill = TFT_WHITE;
    textColor = TFT_BLACK;
  }

  M5.Lcd.startWrite();
  if (customPaintFunction_) {
    customPaintFunction_(this, outline, fill, textColor);
  } else {
    PaintRectangle(fill, outline);
    PaintText(textColor, fill);
  }

  M5.Lcd.endWrite();

  M5.Lcd.setTextStyle(previousTextStyle);
}

TouchButton* nextButton = nullptr;
TouchButton* loopButton = nullptr;
TouchButton* patternControlButton = nullptr;
TouchButton* systemButton = nullptr;
TouchButton* backButton = nullptr;
TouchButton* downButton = nullptr;
TouchButton* upButton = nullptr;
TouchButton* overrideButton = nullptr;
TouchButton* confirmButton = nullptr;
TouchButton* lockButton = nullptr;
TouchButton* shutdownButton = nullptr;
TouchButton* unlock1Button = nullptr;
TouchButton* unlock2Button = nullptr;
TouchButton* ledMinusButton = nullptr;
TouchButton* ledPlusButton = nullptr;
TouchButton* screenMinusButton = nullptr;
TouchButton* screenPlusButton = nullptr;

void SetupButtons() {
  nextButton = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/0, /*w=*/160, /*h=*/60, "Next");
  loopButton = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/60, /*w=*/160, /*h=*/60, "Loop");

  patternControlButton =
      TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/120, /*w=*/160, /*h=*/120, "Pattern Control");
  patternControlButton->SetLabelDatum(/*x_delta=*/0, /*y_delta=*/-25);
  systemButton = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/120, /*w=*/160, /*h=*/120, "System");
  systemButton->SetLabelDatum(/*x_delta=*/0, /*y_delta=*/-25);

  backButton = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/0, /*w=*/160, /*h=*/60, "Back");
  // TODO split the player in half so we can render the selected pattern in the right half of the Back button.
  downButton = TouchButtonManager::Get()->AddButton(/*x=*/80, /*y=*/60, /*w=*/80, /*h=*/60, "Down");
  upButton = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/60, /*w=*/80, /*h=*/60, "Up");
  overrideButton = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/120, /*w=*/160, /*h=*/60, "Override");
  confirmButton = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/180, /*w=*/160, /*h=*/60, "Confirm");
  lockButton = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/180, /*w=*/160, /*h=*/60, "Lock");
  shutdownButton = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/180, /*w=*/160, /*h=*/60, "Shutdown");
  unlock1Button = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/0, /*w=*/160, /*h=*/60, "Unlock");
  unlock2Button = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/180, /*w=*/160, /*h=*/60, "Unlock");
  ledMinusButton = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/0, /*w=*/80, /*h=*/60, "LED-");
  ledPlusButton = TouchButtonManager::Get()->AddButton(/*x=*/240, /*y=*/0, /*w=*/80, /*h=*/60, "LED+");
  screenMinusButton = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/60, /*w=*/80, /*h=*/60, "Screen-");
  screenPlusButton = TouchButtonManager::Get()->AddButton(/*x=*/240, /*y=*/60, /*w=*/80, /*h=*/60, "Screen+");
}

std::string gCurrentPatternName;
constexpr Milliseconds kLockDelay = 60000;
constexpr Milliseconds kUnlockingTime = 5000;
Milliseconds gLastScreenInteractionTime = -1;

class PatternControlMenu {
 public:
  enum class State {
    kOff,
    kPattern,
    kPalette,
    kColor,
    kConfirmed,
  };
  void draw() {
    // Temporarily hide the confirm button so we can set the right string before painting it.
    confirmButton->Hide();
    TouchButtonManager::Get()->MaybePaint();
    // Reset text datum and color in case we need to draw any.
    M5.Lcd.setTextDatum(TL_DATUM);  // Top Left.
    M5.Lcd.setTextColor(WHITE, BLACK);
    switch (state_) {
      case State::kOff:  // Fall through.
      case State::kPattern: {
        state_ = State::kPattern;
        TouchButtonManager::Get()->RedrawRightHalf();
        if (selectedPatternIndex_ < kNumPatternsFirstPage) {
          for (uint8_t i = 0; i < kNumPatternsFirstPage; i++) {
            drawPatternTextLine(i, kSelectablePatterns[i].name, i == selectedPatternIndex_);
          }
          M5.Lcd.setTextColor(WHITE, BLACK);
          M5.Lcd.drawString("More Patterns...", x_, kNumPatternsFirstPage * dy());
        } else {
          M5.Lcd.setTextColor(WHITE, BLACK);
          M5.Lcd.drawString("Previous Patterns...", x_, /*y=*/0);
          for (uint8_t i = 0; i < kNumPatternsSecondPage; i++) {
            drawPatternTextLine(i + 1, kSelectablePatterns[i + kNumPatternsFirstPage].name,
                                i + kNumPatternsFirstPage == selectedPatternIndex_);
          }
        }
      } break;
      case State::kPalette: {
        TouchButtonManager::Get()->RedrawRightHalf();
        for (uint8_t i = 0; i < kNumPalettes; i++) {
          drawPatternTextLine(i, kPaletteNames[i], i == selectedPaletteIndex_);
        }
      } break;
      case State::kColor: {
        TouchButtonManager::Get()->RedrawRightHalf();
        for (uint8_t i = 0; i < kNumColors; i++) { drawPatternTextLine(i, kColorNames[i], i == selectedColorIndex_); }
      } break;
      case State::kConfirmed: {
        // Do nothing.
      } break;
    }
    confirmButton->Draw(/*force=*/true);
  }
  void downPressed() {
    if (state_ == State::kPattern) {
      if (selectedPatternIndex_ < kNumPatternsFirstPage - 1) {
        drawPatternTextLine(selectedPatternIndex_, kSelectablePatterns[selectedPatternIndex_].name, /*selected=*/false);
        selectedPatternIndex_++;
        drawPatternTextLine(selectedPatternIndex_, kSelectablePatterns[selectedPatternIndex_].name, /*selected=*/true);
        confirmButton->Draw(/*force=*/true);
      } else if (selectedPatternIndex_ == kNumPatternsFirstPage - 1) {
        selectedPatternIndex_++;
        draw();
      } else if (selectedPatternIndex_ < kNumPatternsFirstPage + kNumPatternsSecondPage - 1) {
        drawPatternTextLine(1 + selectedPatternIndex_ - kNumPatternsFirstPage,
                            kSelectablePatterns[selectedPatternIndex_].name,
                            /*selected=*/false);
        selectedPatternIndex_++;
        drawPatternTextLine(1 + selectedPatternIndex_ - kNumPatternsFirstPage,
                            kSelectablePatterns[selectedPatternIndex_].name,
                            /*selected=*/true);
        confirmButton->Draw(/*force=*/true);
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
      } else if (selectedPatternIndex_ < kNumPatternsFirstPage) {
        drawPatternTextLine(selectedPatternIndex_, kSelectablePatterns[selectedPatternIndex_].name,
                            /*selected=*/false);
        selectedPatternIndex_--;
        drawPatternTextLine(selectedPatternIndex_, kSelectablePatterns[selectedPatternIndex_].name,
                            /*selected=*/true);
      } else if (selectedPatternIndex_ == kNumPatternsFirstPage) {
        selectedPatternIndex_--;
        draw();
      } else {
        drawPatternTextLine(1 + selectedPatternIndex_ - kNumPatternsFirstPage,
                            kSelectablePatterns[selectedPatternIndex_].name,
                            /*selected=*/false);
        selectedPatternIndex_--;
        drawPatternTextLine(1 + selectedPatternIndex_ - kNumPatternsFirstPage,
                            kSelectablePatterns[selectedPatternIndex_].name,
                            /*selected=*/true);
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
    confirmButton->Draw(/*force=*/true);
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
    overrideButton->SetHighlight(overrideEnabled_);
    overrideButton->SetLabelText(overrideEnabled_ ? "Override ON" : "Override");
    if (overrideEnabled_) {
      SetOverridePrecedence(player, currentTime);
    } else {
      SetDefaultPrecedence(player, currentTime);
    }
    overrideButton->Draw(/*force=*/true);
  }
  bool confirmPressed(Player& player, Milliseconds currentTime) {
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
  State ConfirmButtonState() const {
    if (state_ == State::kPattern) { return kSelectablePatterns[selectedPatternIndex_].nextState; }
    return State::kConfirmed;
  }

 private:
  bool setPattern(Player& player, PatternBits patternBits, Milliseconds currentTime) {
    patternBits = randomizePatternBits(patternBits);
    player.setPattern(patternBits, currentTime);
    state_ = State::kOff;
    return true;
  }
  bool setPatternWithPalette(Player& player, PatternBits patternBits, uint8_t palette, Milliseconds currentTime) {
    jll_info("%u setPatternWithPalette patternBits=%08x palette=%u combined=%08x", currentTime, patternBits, palette,
             patternBits | (palette << 13));
    if (patternBits == kAllPalettePattern) {  // forced palette.
      player.forcePalette(palette, currentTime);
      state_ = State::kOff;
      return true;
    }
    player.stopForcePalette(currentTime);
    return setPattern(player, patternBits | (palette << 13), currentTime);
  }
  bool setPatternWithColor(Player& player, PatternBits patternBits, uint8_t color, Milliseconds currentTime) {
    player.stopForcePalette(currentTime);
    if (patternBits == 0x0700 && color == 0) {  // glow-black is just solid-black.
      return setPattern(player, 0, currentTime);
    }
    return setPattern(player, patternBits + color * 0x100, currentTime);
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
  struct SelectablePattern {
    const char* name;
    PatternBits bits;
    State nextState;
  };
  static constexpr PatternBits kAllPalettePattern = 0x00000F30;
  static constexpr uint8_t kNumPatternsFirstPage = 4 + 1 + 2;
  static constexpr uint8_t kNumPatternsSecondPage = 3 + 3 + 2;
  // clang-format off
  // Apparently some versions of clang-format disagree on how to format this.
  SelectablePattern kSelectablePatterns[kNumPatternsFirstPage + kNumPatternsSecondPage] = {
      // Main patterns.
      {        "rings",         0x00000001,   State::kPalette},
      {        "flame",         0x40000001,   State::kPalette},
      {  "spin-plasma",         0xC0000001,   State::kPalette},
      {     "hiphotic",         0x80000001,   State::kPalette},
      // All-palette pattern.
      {  "all-palette", kAllPalettePattern,   State::kPalette},
      // Legacy palette patterns.
      {    "metaballs",         0x80000030,   State::kPalette},
      {       "bursts",         0x00000030,   State::kPalette},
      // Legacy non-palette patterns.
      {      "glitter",             0x1200, State::kConfirmed},
      {   "the-matrix",             0x1300, State::kConfirmed},
      {    "threesine",             0x1400, State::kConfirmed},
      // Non-color special patterns.
      {     "synctest",             0x0F00, State::kConfirmed},
      {  "calibration",             0x1000, State::kConfirmed},
      {"follow-strand",             0x1100, State::kConfirmed},
      // CRGB special patterns.
      {        "solid",             0x0000,     State::kColor},
      {         "glow",             0x0700,     State::kColor},
  };
  // clang-format on
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

void drawPatternControlButton(TouchButton* button, int outline, int fill, int textColor) {
  button->PaintRectangle(fill, outline);
  button->PaintText(textColor, fill);
  M5.Lcd.setTextDatum(BC_DATUM);  // Bottom Center.
  M5.Lcd.drawString(gCurrentPatternName.c_str(), /*x=*/80, /*y=*/210);
}

void drawSystemButton(TouchButton* button, int outline, int fill, int textColor) {
  button->PaintRectangle(fill, outline);
  button->PaintText(textColor, fill);
  M5.Lcd.setTextDatum(BC_DATUM);  // Bottom Center.
  M5.Lcd.drawString(BOOT_MESSAGE, /*x=*/240, /*y=*/210);
}

void drawConfirmButton(TouchButton* button, int outline, int fill, int textColor) {
  const char* confirmLabel = "Error ?";
  switch (gPatternControlMenu.ConfirmButtonState()) {
    case PatternControlMenu::State::kOff: confirmLabel = "Error Off"; break;
    case PatternControlMenu::State::kPattern: confirmLabel = "Error Pattern"; break;
    case PatternControlMenu::State::kPalette: confirmLabel = "Select Palette"; break;
    case PatternControlMenu::State::kColor: confirmLabel = "Select CRGB"; break;
    case PatternControlMenu::State::kConfirmed: confirmLabel = "Confirm"; break;
  }
  button->PaintRectangle(fill, outline);
  button->PaintText(textColor, fill, confirmLabel);
}

void DrawMainMenuButtons() {
  unlock1Button->Hide();
  unlock2Button->Hide();
  nextButton->Draw();
  loopButton->Draw();
  patternControlButton->Draw();
  systemButton->Draw();
}

void HideMainMenuButtons() {
  nextButton->Hide();
  loopButton->Hide();
  patternControlButton->Hide();
  systemButton->Hide();
}

void DrawPatternControlMenuButtons() {
  unlock1Button->Hide();
  unlock2Button->Hide();
  backButton->Draw();
  downButton->Draw();
  upButton->Draw();
  overrideButton->Draw();
}

void HidePatternControlMenuButtons() {
  backButton->Hide();
  downButton->Hide();
  upButton->Hide();
  overrideButton->Hide();
  confirmButton->Hide();
}

void DrawSystemMenuButtons() {
  unlock1Button->Hide();
  unlock2Button->Hide();
  backButton->Draw();
  lockButton->Draw();
  shutdownButton->Draw();
  ledPlusButton->Draw();
  ledMinusButton->Draw();
  screenPlusButton->Draw();
  screenMinusButton->Draw();
}

void HideSystemMenuButtons() {
  backButton->Hide();
  lockButton->Hide();
  shutdownButton->Hide();
  ledPlusButton->Hide();
  ledMinusButton->Hide();
  screenPlusButton->Hide();
  screenMinusButton->Hide();
}

void startMainMenu(Player& player, Milliseconds currentTime) {
  gScreenMode = ScreenMode::kMainMenu;
  TouchButtonManager::Get()->Redraw();
  DrawMainMenuButtons();
  core2ScreenRenderer.setEnabled(true);
}

void lockScreen(Milliseconds currentTime) {
  gLastScreenInteractionTime = -1;
  gScreenMode = ScreenMode::kOff;
  unlock1Button->Hide();
  unlock2Button->Hide();
  HideSystemMenuButtons();
  HideMainMenuButtons();
  HidePatternControlMenuButtons();
  core2ScreenRenderer.setEnabled(false);
  SetCore2ScreenBrightness(0);
  TouchButtonManager::Get()->Redraw();
}

void patternControlButtonPressed(Player& player, Milliseconds currentTime) {
  gScreenMode = ScreenMode::kPatternControlMenu;
  gLastScreenInteractionTime = currentTime;
  HideMainMenuButtons();
  core2ScreenRenderer.setEnabled(false);
  TouchButtonManager::Get()->Redraw();
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
    : Esp32Ui(player, currentTime), ledBrightness_(kInitialLedBrightness), onBrightness_(kDefaultOnBrightness) {}

void Core2AwsUi::InitialSetup(Milliseconds currentTime) {
  player_.set_brightness(ledBrightness_);
  auto cfg = M5.config();
  cfg.serial_baudrate = 0;
  M5.begin(cfg);
  if (gScreenMode == ScreenMode::kOff) {
    SetCore2ScreenBrightness(0);
  } else {
    SetCore2ScreenBrightness(onBrightness_);
  }
  TouchButtonManager::Get()->Redraw();
  SetupButtons();
  HidePatternControlMenuButtons();
  HideSystemMenuButtons();
  unlock1Button->Hide();
  unlock2Button->Hide();
  patternControlButton->SetCustomPaintFunction(drawPatternControlButton);
  systemButton->SetCustomPaintFunction(drawSystemButton);
  confirmButton->SetCustomPaintFunction(drawConfirmButton);
  player_.addStrand(kCore2ScreenPixels, core2ScreenRenderer);
  SetDefaultPrecedence(player_, currentTime);
}

void Core2AwsUi::FinalSetup(Milliseconds currentTime) {
  TouchButtonManager::Get()->MaybePaint();
  gCurrentPatternName = player_.currentEffectName();
  if (gScreenMode == ScreenMode::kMainMenu) {
    startMainMenu(player_, currentTime);
  } else {
    HideMainMenuButtons();
    core2ScreenRenderer.setEnabled(false);
  }
}

void Core2AwsUi::DrawSystemTextLines(Milliseconds currentTime) {
  TouchButtonManager::Get()->MaybePaint();
  size_t i = 0;
  char line[100] = {};
  // LED Brighness.
  snprintf(line, sizeof(line) - 1, "LED Brgt %u/255", ledBrightness_);
  DrawSystemTextLine(i++, line);
  // Screen Brightness.
  snprintf(line, sizeof(line) - 1, "Scrn Brgt %u/%u", onBrightness_, kMaxOnBrightness);
  DrawSystemTextLine(i++, line);
  // BLE.
  snprintf(line, sizeof(line) - 1, "BLE: %s", Esp32BleNetwork::get()->getStatusStr(currentTime).c_str());
  DrawSystemTextLine(i++, line);
#if JL_WIFI
  // Wi-Fi.
  snprintf(line, sizeof(line) - 1, "Wi-Fi: %s", WiFiNetwork::get()->getStatusStr(currentTime).c_str());
  DrawSystemTextLine(i++, line);
#endif  // JL_WIFI
  // Other.
  if (player_.following() == NetworkType::kLeading) {
    snprintf(line, sizeof(line) - 1, "Leading");
  } else {
    snprintf(line, sizeof(line) - 1, "Following %s nh=%u", NetworkTypeToString(player_.following()),
             player_.currentNumHops());
  }
  DrawSystemTextLine(i++, line);
}

void Core2AwsUi::RunLoop(Milliseconds currentTime) {
  M5.update();
  auto touchDetail = M5.Touch.getDetail();
  if (touchDetail.isPressed()) {
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t totalHeap = ESP.getHeapSize();
    // TODO enable PSRAM by adding "-DBOARD_HAS_PSRAM" and "-mfix-esp32-psram-cache-issue" to build_flags in
    // platformio.ini https://thingpulse.com/esp32-how-to-use-psram/
    uint32_t freePSRAM = ESP.getFreePsram();
    uint32_t totalPSRAM = ESP.getPsramSize();
    int16_t px = touchDetail.x;
    int16_t py = touchDetail.y;
    jll_info("%u background pressed x=%d y=%d, free RAM %u/%u free PSRAM %u/%u", currentTime, px, py,
             static_cast<unsigned int>(freeHeap), static_cast<unsigned int>(totalHeap),
             static_cast<unsigned int>(freePSRAM), static_cast<unsigned int>(totalPSRAM));
    bool buttonPressed = TouchButtonManager::Get()->HandlePress(px, py);
    if (!buttonPressed) {
      switch (gScreenMode) {
        case ScreenMode::kOff: {
          jll_info("RunLoop kOff");
          gLastScreenInteractionTime = currentTime;
          SetCore2ScreenBrightness(onBrightness_);
#if JL_BUTTON_LOCK
          jll_info("%u starting unlock sequence from button press", currentTime);
          gScreenMode = ScreenMode::kLocked1;
          unlock2Button->Hide();
          unlock1Button->Draw();
#else   // JL_BUTTON_LOCK
          jll_info("%u unlocking from button press", currentTime);
          startMainMenu(player_, currentTime);
          gLastScreenInteractionTime = currentTime;
#endif  // JL_BUTTON_LOCK
        } break;
        case ScreenMode::kMainMenu: {
          if (px < 160 && py < 120) {
            gScreenMode = ScreenMode::kFullScreenPattern;
            jll_info("%u pattern screen pressed", currentTime);
            HideMainMenuButtons();
            TouchButtonManager::Get()->Redraw();
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
        case ScreenMode::kLocked1: {
          jll_info("%u ignoring background press during unlock1", currentTime);
        } break;
        case ScreenMode::kLocked2: {
          jll_info("%u locking screen due to background press while unlocking", currentTime);
          lockScreen(currentTime);
        } break;
      }
    }
  } else {
    TouchButtonManager::Get()->HandleIdle();
  }
  if (nextButton->JustReleased()) {
    jll_info("%u next pressed", currentTime);
    if (gScreenMode == ScreenMode::kMainMenu) {
      gLastScreenInteractionTime = currentTime;
      player_.next(currentTime);
    }
  }
  if (loopButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kMainMenu) {
      jll_info("%u loop pressed", currentTime);
      gLastScreenInteractionTime = currentTime;
      if (player_.isLooping()) {
        player_.stopLooping(currentTime);
        loopButton->SetLabelText("Loop");
        loopButton->SetHighlight(false);
      } else {
        player_.loopOne(currentTime);
        loopButton->SetLabelText("Looping");
        loopButton->SetHighlight(true);
      }
      loopButton->Draw(/*force=*/true);
    } else {
      jll_info("%u ignoring loop pressed", currentTime);
    }
  }
  if (patternControlButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kMainMenu) {
      jll_info("%u pattern control button pressed", currentTime);
      patternControlButtonPressed(player_, currentTime);
    } else {
      jll_info("%u ignoring pattern control button pressed", currentTime);
    }
  }
  if (systemButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kMainMenu) {
      jll_info("%u system button pressed", currentTime);
      gScreenMode = ScreenMode::kSystemMenu;
      gLastScreenInteractionTime = currentTime;
      HideMainMenuButtons();
      core2ScreenRenderer.setEnabled(false);
      TouchButtonManager::Get()->Redraw();
      DrawSystemMenuButtons();
      DrawSystemTextLines(currentTime);
    } else {
      jll_info("%u ignoring system button pressed", currentTime);
    }
  }
  if (backButton->JustReleased()) {
    if ((gScreenMode == ScreenMode::kSystemMenu || gScreenMode == ScreenMode::kPatternControlMenu) &&
        gPatternControlMenu.backPressed()) {
      jll_info("%u back button pressed", currentTime);
      gLastScreenInteractionTime = currentTime;
      HidePatternControlMenuButtons();
      HideSystemMenuButtons();
      startMainMenu(player_, currentTime);
    } else {
      jll_info("%u ignoring back button pressed", currentTime);
    }
  }
  if (downButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kPatternControlMenu) {
      jll_info("%u down button pressed", currentTime);
      gLastScreenInteractionTime = currentTime;
      gPatternControlMenu.downPressed();
    } else {
      jll_info("%u ignoring down button pressed", currentTime);
    }
  }
  if (upButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kPatternControlMenu) {
      jll_info("%u up button pressed", currentTime);
      gLastScreenInteractionTime = currentTime;
      gPatternControlMenu.upPressed();
    } else {
      jll_info("%u ignoring up button pressed", currentTime);
    }
  }
  if (overrideButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kPatternControlMenu) {
      jll_info("%u override button pressed", currentTime);
      gLastScreenInteractionTime = currentTime;
      gPatternControlMenu.overridePressed(player_, currentTime);
    } else {
      jll_info("%u ignoring override button pressed", currentTime);
    }
  }
  if (confirmButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kPatternControlMenu) {
      jll_info("%u confirm button pressed", currentTime);
      confirmButtonPressed(player_, currentTime);
    } else {
      jll_info("%u ignoring confirm button pressed", currentTime);
    }
  }
  if (lockButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kSystemMenu) {
      jll_info("%u lock button pressed", currentTime);
      lockScreen(currentTime);
    } else {
      jll_info("%u ignoring lock button pressed", currentTime);
    }
  }
  if (shutdownButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kSystemMenu) {
      jll_info("%u shutdown button pressed", currentTime);
      CorePowerOff();
    } else {
      jll_info("%u ignoring shutdown button pressed", currentTime);
    }
  }
  if (unlock2Button->JustReleased()) {
    gLastScreenInteractionTime = currentTime;
    if (gScreenMode == ScreenMode::kLocked2) {
      jll_info("%u unlock2 button pressed", currentTime);
      unlock2Button->Hide();
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
      CorePowerOff();
    } else {
      jll_info("%u ignoring unlock2 button pressed", currentTime);
    }
  }
  if (unlock1Button->JustReleased()) {
    gLastScreenInteractionTime = currentTime;
    if (gScreenMode == ScreenMode::kLocked1) {
      jll_info("%u unlock1 button pressed", currentTime);
      gScreenMode = ScreenMode::kLocked2;
      unlock1Button->Hide();
      TouchButtonManager::Get()->Redraw();
      unlock2Button->Draw();
    } else if (gScreenMode == ScreenMode::kMainMenu) {
      jll_info("%u unlock1 button unexpectedly pressed in main menu, treating as next button", currentTime);
      player_.next(currentTime);
    } else {
      jll_info("%u ignoring unlock1 button pressed", currentTime);
    }
  }
  if (ledPlusButton->JustReleased()) {
    jll_info("%u ledPlusButton button pressed", currentTime);
    if (ledBrightness_ < 255 && gScreenMode == ScreenMode::kSystemMenu) {
      ledBrightness_++;
      jll_info("%u setting LED brightness to %u", currentTime, ledBrightness_);
      player_.set_brightness(ledBrightness_);
      DrawSystemTextLines(currentTime);
    }
  }
  if (ledMinusButton->JustReleased()) {
    jll_info("%u ledMinusButton button pressed", currentTime);
    if (ledBrightness_ > 0 && gScreenMode == ScreenMode::kSystemMenu) {
      ledBrightness_--;
      jll_info("%u setting LED brightness to %u", currentTime, ledBrightness_);
      player_.set_brightness(ledBrightness_);
      DrawSystemTextLines(currentTime);
    }
  }
  if (screenPlusButton->JustReleased()) {
    jll_info("%u screenPlusButton button pressed", currentTime);
    if (onBrightness_ < kMaxOnBrightness && gScreenMode == ScreenMode::kSystemMenu) {
      onBrightness_++;
      SetCore2ScreenBrightness(onBrightness_);
      DrawSystemTextLines(currentTime);
    }
  }
  if (screenMinusButton->JustReleased()) {
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
      patternControlButton->Draw(/*force=*/true);
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
