#include "jazzlights/ui/ui_core2.h"

#include "jazzlights/util/config.h"

#if JL_IS_CONTROLLER(CORE2AWS) || JL_IS_CONTROLLER(CORES3)

#include <M5Unified.h>

#include "jazzlights/layout/matrix.h"
#include "jazzlights/network/esp32_ble.h"
#include "jazzlights/network/wifi.h"
#include "jazzlights/orrery/orrery_common.h"
#include "jazzlights/ui/touch_button.h"

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
  kOrreryMenu,
};
#if JL_BUTTON_LOCK
static constexpr ScreenMode kInitialScreenMode = ScreenMode::kOff;
#else   // JL_BUTTON_LOCK
static constexpr ScreenMode kInitialScreenMode = ScreenMode::kMainMenu;
#endif  // JL_BUTTON_LOCK

#ifndef JL_CORE_CAN_POWER_OFF
#define JL_CORE_CAN_POWER_OFF 0
#endif  // JL_CORE_CAN_POWER_OFF

void CorePowerOff() {
#if JL_CORE_CAN_POWER_OFF
  M5.Power.powerOff();
#endif  // JL_CORE_CAN_POWER_OFF
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

void SetDefaultPrecedence(Player& player) { player.UpdatePrecedence(4000, 1000); }

void SetOverridePrecedence(Player& player) { player.UpdatePrecedence(kDefaultOverridePrecedence, 5000); }

void DrawSystemTextLine(uint8_t i, const char* text) {
  constexpr uint8_t kSytemLineHeight = 22;
  constexpr uint8_t kSytemStartX = 5;
  constexpr uint8_t kSytemStartY = 65;
  M5.Display.setTextDatum(TL_DATUM);  // Top Left.
  const uint16_t x = kSytemStartX;
  const uint16_t y = kSytemStartY + i * kSytemLineHeight;
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.fillRect(x, y, /*w=*/155, /*h=*/kSytemLineHeight, BLACK);
  M5.Display.drawString(text, x, y);
}

static const Matrix kCore2ScreenPixels(40, 30);

class Core2ScreenRenderer : public Renderer {
 public:
  Core2ScreenRenderer() {}
  void SetFullScreen(bool fullScreen) { fullScreen_ = fullScreen; }
  void ToggleEnabled() { SetEnabled(!enabled_); }
  void SetEnabled(bool enabled) { enabled_ = enabled; }
  void RenderPixel(size_t index, CRGB color) override {
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
      bool swap = M5.Display.getSwapBytes();
      M5.Display.setSwapBytes(true);
      M5.Display.pushImage(/*x0=*/0, /*y0=*/y * factor, /*w=*/40 * factor, /*h=*/factor, rowColors16_);
      M5.Display.setSwapBytes(swap);
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
Core2ScreenRenderer gCore2ScreenRenderer;

TouchButton* gNextButton = nullptr;
TouchButton* gLoopButton = nullptr;
TouchButton* gPatternControlButton = nullptr;
TouchButton* gOrreryButton = nullptr;
TouchButton* gSystemButton = nullptr;
TouchButton* gBackButton = nullptr;
TouchButton* gDownButton = nullptr;
TouchButton* gUpButton = nullptr;
TouchButton* gOverrideButton = nullptr;
TouchButton* gConfirmButton = nullptr;
TouchButton* gLockButton = nullptr;
TouchButton* gShutdownButton = nullptr;
TouchButton* gUnlock1Button = nullptr;
TouchButton* gUnlock2Button = nullptr;
TouchButton* gLedMinusButton = nullptr;
TouchButton* gLedPlusButton = nullptr;
TouchButton* gScreenMinusButton = nullptr;
TouchButton* gScreenPlusButton = nullptr;

void SetupButtons() {
  gNextButton = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/0, /*w=*/160, /*h=*/60, "Next");
  gLoopButton = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/60, /*w=*/160, /*h=*/60, "Loop");

  gPatternControlButton =
      TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/120, /*w=*/160, /*h=*/120, "Pattern Control");
  gPatternControlButton->SetLabelDatum(/*xDelta=*/0, /*yDelta=*/-25);
  gOrreryButton = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/120, /*w=*/160, /*h=*/60, "Orrery");
  gSystemButton = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/180, /*w=*/160, /*h=*/60, "System");

  gBackButton = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/0, /*w=*/160, /*h=*/60, "Back");
  // TODO split the player in half so we can render the selected pattern in the right half of the Back button.
  gDownButton = TouchButtonManager::Get()->AddButton(/*x=*/80, /*y=*/60, /*w=*/80, /*h=*/60, "Down");
  gUpButton = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/60, /*w=*/80, /*h=*/60, "Up");
  gOverrideButton = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/120, /*w=*/160, /*h=*/60, "Override");
  gConfirmButton = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/180, /*w=*/160, /*h=*/60, "Confirm");
  gLockButton = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/180, /*w=*/160, /*h=*/60, "Lock");
  gShutdownButton = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/180, /*w=*/160, /*h=*/60, "Shutdown");
  gUnlock1Button = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/0, /*w=*/160, /*h=*/60, "Unlock");
  gUnlock2Button = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/180, /*w=*/160, /*h=*/60, "Unlock");
  gLedMinusButton = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/0, /*w=*/80, /*h=*/60, "LED-");
  gLedPlusButton = TouchButtonManager::Get()->AddButton(/*x=*/240, /*y=*/0, /*w=*/80, /*h=*/60, "LED+");
  gScreenMinusButton = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/60, /*w=*/80, /*h=*/60, "Screen-");
  gScreenPlusButton = TouchButtonManager::Get()->AddButton(/*x=*/240, /*y=*/60, /*w=*/80, /*h=*/60, "Screen+");
}

std::string gCurrentPatternName;
constexpr Microseconds kLockDelay = 60000000;
constexpr Microseconds kUnlockingTime = 5000000;
OptionalMicroseconds gLastScreenInteractionTime;

class PatternControlMenu {
 public:
  enum class State {
    kOff,
    kPattern,
    kPalette,
    kColor,
    kConfirmed,
  };
  void Draw() {
    // Temporarily hide the confirm button so we can set the right string before painting it.
    gConfirmButton->Hide();
    TouchButtonManager::Get()->MaybePaint();
    // Reset text datum and color in case we need to draw any.
    M5.Display.setTextDatum(TL_DATUM);  // Top Left.
    M5.Display.setTextColor(WHITE, BLACK);
    switch (state_) {
      case State::kOff:  // Fall through.
      case State::kPattern: {
        state_ = State::kPattern;
        TouchButtonManager::Get()->RedrawRightHalf();
        if (selectedPatternIndex_ < kNumPatternsFirstPage) {
          for (uint8_t i = 0; i < kNumPatternsFirstPage; i++) {
            DrawPatternTextLine(i, selectablePatterns_[i].name, i == selectedPatternIndex_);
          }
          M5.Display.setTextColor(WHITE, BLACK);
          M5.Display.drawString("More Patterns...", x_, kNumPatternsFirstPage * Dy());
        } else {
          M5.Display.setTextColor(WHITE, BLACK);
          M5.Display.drawString("Previous Patterns...", x_, /*y=*/0);
          for (uint8_t i = 0; i < kNumPatternsSecondPage; i++) {
            DrawPatternTextLine(i + 1, selectablePatterns_[i + kNumPatternsFirstPage].name,
                                i + kNumPatternsFirstPage == selectedPatternIndex_);
          }
        }
      } break;
      case State::kPalette: {
        TouchButtonManager::Get()->RedrawRightHalf();
        for (uint8_t i = 0; i < kNumPalettes; i++) {
          DrawPatternTextLine(i, paletteNames_[i], i == selectedPaletteIndex_);
        }
      } break;
      case State::kColor: {
        TouchButtonManager::Get()->RedrawRightHalf();
        for (uint8_t i = 0; i < kNumColors; i++) { DrawPatternTextLine(i, colorNames_[i], i == selectedColorIndex_); }
      } break;
      case State::kConfirmed: {
        // Do nothing.
      } break;
    }
    gConfirmButton->Draw(/*force=*/true);
  }
  void DownPressed() {
    if (state_ == State::kPattern) {
      if (selectedPatternIndex_ < kNumPatternsFirstPage - 1) {
        DrawPatternTextLine(selectedPatternIndex_, selectablePatterns_[selectedPatternIndex_].name, /*selected=*/false);
        selectedPatternIndex_++;
        DrawPatternTextLine(selectedPatternIndex_, selectablePatterns_[selectedPatternIndex_].name, /*selected=*/true);
        gConfirmButton->Draw(/*force=*/true);
      } else if (selectedPatternIndex_ == kNumPatternsFirstPage - 1) {
        selectedPatternIndex_++;
        Draw();
      } else if (selectedPatternIndex_ < kNumPatternsFirstPage + kNumPatternsSecondPage - 1) {
        DrawPatternTextLine(1 + selectedPatternIndex_ - kNumPatternsFirstPage,
                            selectablePatterns_[selectedPatternIndex_].name,
                            /*selected=*/false);
        selectedPatternIndex_++;
        DrawPatternTextLine(1 + selectedPatternIndex_ - kNumPatternsFirstPage,
                            selectablePatterns_[selectedPatternIndex_].name,
                            /*selected=*/true);
        gConfirmButton->Draw(/*force=*/true);
      }
    } else if (state_ == State::kPalette) {
      if (selectedPaletteIndex_ < kNumPalettes - 1) {
        DrawPatternTextLine(selectedPaletteIndex_, paletteNames_[selectedPaletteIndex_], /*selected=*/false);
        selectedPaletteIndex_++;
        DrawPatternTextLine(selectedPaletteIndex_, paletteNames_[selectedPaletteIndex_], /*selected=*/true);
      }
    } else if (state_ == State::kColor) {
      if (selectedColorIndex_ < kNumColors - 1) {
        DrawPatternTextLine(selectedColorIndex_, colorNames_[selectedColorIndex_], /*selected=*/false);
        selectedColorIndex_++;
        DrawPatternTextLine(selectedColorIndex_, colorNames_[selectedColorIndex_], /*selected=*/true);
      }
    }
  }
  void UpPressed() {
    if (state_ == State::kPattern) {
      if (selectedPatternIndex_ == 0) {
        // Do nothing.
      } else if (selectedPatternIndex_ < kNumPatternsFirstPage) {
        DrawPatternTextLine(selectedPatternIndex_, selectablePatterns_[selectedPatternIndex_].name,
                            /*selected=*/false);
        selectedPatternIndex_--;
        DrawPatternTextLine(selectedPatternIndex_, selectablePatterns_[selectedPatternIndex_].name,
                            /*selected=*/true);
      } else if (selectedPatternIndex_ == kNumPatternsFirstPage) {
        selectedPatternIndex_--;
        Draw();
      } else {
        DrawPatternTextLine(1 + selectedPatternIndex_ - kNumPatternsFirstPage,
                            selectablePatterns_[selectedPatternIndex_].name,
                            /*selected=*/false);
        selectedPatternIndex_--;
        DrawPatternTextLine(1 + selectedPatternIndex_ - kNumPatternsFirstPage,
                            selectablePatterns_[selectedPatternIndex_].name,
                            /*selected=*/true);
      }
    } else if (state_ == State::kPalette) {
      if (selectedPaletteIndex_ > 0) {
        DrawPatternTextLine(selectedPaletteIndex_, paletteNames_[selectedPaletteIndex_], /*selected=*/false);
        selectedPaletteIndex_--;
        DrawPatternTextLine(selectedPaletteIndex_, paletteNames_[selectedPaletteIndex_], /*selected=*/true);
      }
    } else if (state_ == State::kColor) {
      if (selectedColorIndex_ > 0) {
        DrawPatternTextLine(selectedColorIndex_, colorNames_[selectedColorIndex_], /*selected=*/false);
        selectedColorIndex_--;
        DrawPatternTextLine(selectedColorIndex_, colorNames_[selectedColorIndex_], /*selected=*/true);
      }
    }
    gConfirmButton->Draw(/*force=*/true);
  }
  bool BackPressed() {
    if (state_ == State::kPattern) {
      state_ = State::kOff;
      return true;
    }
    state_ = State::kPattern;
    Draw();
    return false;
  }
  void OverridePressed(Player& player) {
    overrideEnabled_ = !overrideEnabled_;
    gOverrideButton->SetHighlight(overrideEnabled_);
    gOverrideButton->SetLabelText(overrideEnabled_ ? "Override ON" : "Override");
    if (overrideEnabled_) {
      SetOverridePrecedence(player);
    } else {
      SetDefaultPrecedence(player);
    }
    gOverrideButton->Draw(/*force=*/true);
  }
  bool ConfirmPressed(Player& player) {
    if (state_ == State::kPattern) {
      State nextState = selectablePatterns_[selectedPatternIndex_].nextState;
      if (nextState == State::kPalette || nextState == State::kColor) {
        jll_info("Pattern %s confirmed now asking for %s", selectablePatterns_[selectedPatternIndex_].name,
                 (nextState == State::kPalette ? "palette" : "color"));
        state_ = nextState;
        Draw();
      } else {
        jll_info("Pattern %s confirmed now playing", selectablePatterns_[selectedPatternIndex_].name);
        player.StopForcePalette();
        return SetPattern(player, selectablePatterns_[selectedPatternIndex_].bits);
      }
    } else if (state_ == State::kPalette) {
      jll_info("Pattern %s and palette %s confirmed now playing", selectablePatterns_[selectedPatternIndex_].name,
               paletteNames_[selectedPaletteIndex_]);
      return SetPatternWithPalette(player, selectablePatterns_[selectedPatternIndex_].bits, selectedPaletteIndex_);
    } else if (state_ == State::kColor) {
      jll_info("Pattern %s and color %s confirmed now playing", selectablePatterns_[selectedPatternIndex_].name,
               colorNames_[selectedColorIndex_]);
      return SetPatternWithColor(player, selectablePatterns_[selectedPatternIndex_].bits, selectedColorIndex_);
    }
    return false;
  }
  State ConfirmButtonState() const {
    if (state_ == State::kPattern) { return selectablePatterns_[selectedPatternIndex_].nextState; }
    return State::kConfirmed;
  }

 private:
  bool SetPattern(Player& player, PatternBits patternBits) {
    patternBits = RandomizePatternBits(patternBits);
    player.SetPattern(patternBits);
    state_ = State::kOff;
    return true;
  }
  bool SetPatternWithPalette(Player& player, PatternBits patternBits, uint8_t palette) {
    jll_info("SetPatternWithPalette patternBits=%08x palette=%u combined=%08x", patternBits, palette,
             patternBits | (palette << 13));
    if (patternBits == kAllPalettePattern) {  // forced palette.
      player.ForcePalette(palette);
      state_ = State::kOff;
      return true;
    }
    player.StopForcePalette();
    return SetPattern(player, patternBits | (palette << 13));
  }
  bool SetPatternWithColor(Player& player, PatternBits patternBits, uint8_t color) {
    player.StopForcePalette();
    if (patternBits == 0x0700 && color == 0) {  // glow-black is just solid-black.
      return SetPattern(player, 0);
    }
    return SetPattern(player, patternBits + color * 0x100);
  }
  uint8_t Dy() {
    if (dy_ == 0) { dy_ = M5.Display.fontHeight(); }  // By default this is 22.
    return dy_;
  }
  void DrawPatternTextLine(uint8_t i, const char* text, bool selected) {
    M5.Display.setTextDatum(TL_DATUM);  // Top Left.
    const uint16_t y = i * Dy();
    const uint16_t textColor = selected ? BLACK : WHITE;
    const uint16_t backgroundColor = selected ? WHITE : BLACK;
    M5.Display.setTextColor(textColor, backgroundColor);
    M5.Display.fillRect(x_, y, /*w=*/155, /*h=*/Dy(), backgroundColor);
    M5.Display.drawString(text, x_, y);
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
  SelectablePattern selectablePatterns_[kNumPatternsFirstPage + kNumPatternsSecondPage] = {
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
  const char* paletteNames_[kNumPalettes] = {
      "heat", "lava", "ocean", "cloud", "party", "forest", "rainbow",
  };
  static constexpr size_t kNumColors = 8;
  const char* colorNames_[kNumColors] = {
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

class OrreryMenu {
 public:
  void Draw() {
    // Reset text datum and color in case we need to draw any.
    M5.Display.setTextDatum(TL_DATUM);  // Top Left.
    M5.Display.setTextColor(WHITE, BLACK);
    TouchButtonManager::Get()->RedrawRightHalf();
    if (selectedSceneIndex_ < kNumScenesFirstPage) {
      for (uint8_t i = 0; i < kNumScenesFirstPage && i < kNumScenes; i++) {
        OrreryScene scene = static_cast<OrreryScene>(i + static_cast<int>(OrreryScene::kMinScene));
        DrawSceneTextLine(i, OrrerySceneToString(scene), i == selectedSceneIndex_);
      }
      if (kNumScenes > kNumScenesFirstPage) {
        M5.Display.setTextColor(WHITE, BLACK);
        M5.Display.drawString("More...", x_, kNumScenesFirstPage * Dy());
      }
    } else {
      M5.Display.setTextColor(WHITE, BLACK);
      M5.Display.drawString("Back...", x_, 0);
      for (uint8_t i = 0; i < kNumScenes - kNumScenesFirstPage; i++) {
        uint8_t idx = i + kNumScenesFirstPage;
        OrreryScene scene = static_cast<OrreryScene>(idx + static_cast<int>(OrreryScene::kMinScene));
        DrawSceneTextLine(i + 1, OrrerySceneToString(scene), idx == selectedSceneIndex_);
      }
    }
    gConfirmButton->Draw(/*force=*/true);
  }
  void DownPressed() {
    if (selectedSceneIndex_ < kNumScenes - 1) {
      if (selectedSceneIndex_ == kNumScenesFirstPage - 1) {
        selectedSceneIndex_++;
        Draw();
      } else {
        uint8_t lineIdx = selectedSceneIndex_ < kNumScenesFirstPage ? selectedSceneIndex_
                                                                    : (selectedSceneIndex_ - kNumScenesFirstPage + 1);
        OrreryScene oldScene = static_cast<OrreryScene>(selectedSceneIndex_ + static_cast<int>(OrreryScene::kMinScene));
        DrawSceneTextLine(lineIdx, OrrerySceneToString(oldScene), /*selected=*/false);
        selectedSceneIndex_++;
        OrreryScene newScene = static_cast<OrreryScene>(selectedSceneIndex_ + static_cast<int>(OrreryScene::kMinScene));
        DrawSceneTextLine(lineIdx + 1, OrrerySceneToString(newScene), /*selected=*/true);
      }
    }
  }
  void UpPressed() {
    if (selectedSceneIndex_ > 0) {
      if (selectedSceneIndex_ == kNumScenesFirstPage) {
        selectedSceneIndex_--;
        Draw();
      } else {
        uint8_t lineIdx = selectedSceneIndex_ < kNumScenesFirstPage ? selectedSceneIndex_
                                                                    : (selectedSceneIndex_ - kNumScenesFirstPage + 1);
        OrreryScene oldScene = static_cast<OrreryScene>(selectedSceneIndex_ + static_cast<int>(OrreryScene::kMinScene));
        DrawSceneTextLine(lineIdx, OrrerySceneToString(oldScene), /*selected=*/false);
        selectedSceneIndex_--;
        OrreryScene newScene = static_cast<OrreryScene>(selectedSceneIndex_ + static_cast<int>(OrreryScene::kMinScene));
        DrawSceneTextLine(lineIdx - 1, OrrerySceneToString(newScene), /*selected=*/true);
      }
    }
  }
  bool ConfirmPressed(Player& player) {
    OrreryScene scene = static_cast<OrreryScene>(selectedSceneIndex_ + static_cast<int>(OrreryScene::kMinScene));
    player.SetOrrerySceneIdToSend(static_cast<OrrerySceneId>(scene));
    return true;
  }

 private:
  uint8_t Dy() {
    if (dy_ == 0) { dy_ = M5.Display.fontHeight(); }
    return dy_;
  }
  void DrawSceneTextLine(uint8_t i, const char* text, bool selected) {
    M5.Display.setTextDatum(TL_DATUM);  // Top Left.
    const uint16_t y = i * Dy();
    const uint16_t textColor = selected ? BLACK : WHITE;
    const uint16_t backgroundColor = selected ? WHITE : BLACK;
    M5.Display.setTextColor(textColor, backgroundColor);
    M5.Display.fillRect(x_, y, /*w=*/155, /*h=*/Dy(), backgroundColor);
    M5.Display.drawString(text, x_, y);
  }
  static constexpr uint8_t kNumScenes =
      static_cast<int>(OrreryScene::kMaxScene) - static_cast<int>(OrreryScene::kMinScene) + 1;
  static constexpr uint8_t kNumScenesFirstPage = 10;
  uint8_t selectedSceneIndex_ = 0;
  const int16_t x_ = 165;
  uint8_t dy_ = 0;
};

OrreryMenu gOrreryMenu;

void DrawPatternControlButton(TouchButton* button, int outline, int fill, int textColor) {
  button->PaintRectangle(fill, outline);
  button->PaintText(textColor, fill);
  M5.Display.setTextDatum(BC_DATUM);  // Bottom Center.
  M5.Display.drawString(gCurrentPatternName.c_str(), /*x=*/80, /*y=*/210);
}

OrreryScene gOrreryScene = OrreryScene::kInvalidScene;
void DrawOrreryButton(TouchButton* button, int outline, int fill, int textColor) {
  button->PaintRectangle(fill, outline);
  M5.Display.setTextDatum(BC_DATUM);  // Bottom Center.
  M5.Display.setTextColor(textColor, fill);
  M5.Display.drawString("Orrery", /*x=*/240, /*y=*/145);
  M5.Display.drawString(OrrerySceneToString(gOrreryScene), /*x=*/240, /*y=*/170);
}

void DrawSystemButton(TouchButton* button, int outline, int fill, int textColor) {
  button->PaintRectangle(fill, outline);
  M5.Display.setTextDatum(BC_DATUM);  // Bottom Center.
  M5.Display.setTextColor(textColor, fill);
  M5.Display.drawString("System", /*x=*/240, /*y=*/205);
  M5.Display.drawString(BOOT_MESSAGE, /*x=*/240, /*y=*/230);
}

void DrawConfirmButton(TouchButton* button, int outline, int fill, int textColor) {
  const char* confirmLabel = "Error ?";
  if (gScreenMode == ScreenMode::kOrreryMenu) {
    confirmLabel = "Confirm";
  } else {
    switch (gPatternControlMenu.ConfirmButtonState()) {
      case PatternControlMenu::State::kOff: confirmLabel = "Error Off"; break;
      case PatternControlMenu::State::kPattern: confirmLabel = "Error Pattern"; break;
      case PatternControlMenu::State::kPalette: confirmLabel = "Select Palette"; break;
      case PatternControlMenu::State::kColor: confirmLabel = "Select CRGB"; break;
      case PatternControlMenu::State::kConfirmed: confirmLabel = "Confirm"; break;
    }
  }
  button->PaintRectangle(fill, outline);
  button->PaintText(textColor, fill, confirmLabel);
}

void DrawMainMenuButtons() {
  gUnlock1Button->Hide();
  gUnlock2Button->Hide();
  gNextButton->Draw();
  gLoopButton->Draw();
  gPatternControlButton->Draw();
  gOrreryButton->Draw();
  gSystemButton->Draw();
}

void HideMainMenuButtons() {
  gNextButton->Hide();
  gLoopButton->Hide();
  gPatternControlButton->Hide();
  gOrreryButton->Hide();
  gSystemButton->Hide();
}

void DrawPatternControlMenuButtons() {
  gUnlock1Button->Hide();
  gUnlock2Button->Hide();
  gBackButton->Draw();
  gDownButton->Draw();
  gUpButton->Draw();
  gOverrideButton->Draw();
}

void HidePatternControlMenuButtons() {
  gBackButton->Hide();
  gDownButton->Hide();
  gUpButton->Hide();
  gOverrideButton->Hide();
  gConfirmButton->Hide();
}

void DrawOrreryMenuButtons() {
  gUnlock1Button->Hide();
  gUnlock2Button->Hide();
  gBackButton->Draw();
  gDownButton->Draw();
  gUpButton->Draw();
}

void HideOrreryMenuButtons() {
  gBackButton->Hide();
  gDownButton->Hide();
  gUpButton->Hide();
  gConfirmButton->Hide();
}

void DrawSystemMenuButtons() {
  gUnlock1Button->Hide();
  gUnlock2Button->Hide();
  gBackButton->Draw();
  gLockButton->Draw();
  gShutdownButton->Draw();
  gLedPlusButton->Draw();
  gLedMinusButton->Draw();
  gScreenPlusButton->Draw();
  gScreenMinusButton->Draw();
}

void HideSystemMenuButtons() {
  gBackButton->Hide();
  gLockButton->Hide();
  gShutdownButton->Hide();
  gLedPlusButton->Hide();
  gLedMinusButton->Hide();
  gScreenPlusButton->Hide();
  gScreenMinusButton->Hide();
}

void StartMainMenu(Player& player) {
  gScreenMode = ScreenMode::kMainMenu;
  TouchButtonManager::Get()->Redraw();
  DrawMainMenuButtons();
  gCore2ScreenRenderer.SetEnabled(true);
}

void LockScreen() {
  gLastScreenInteractionTime.reset();
  gScreenMode = ScreenMode::kOff;
  gUnlock1Button->Hide();
  gUnlock2Button->Hide();
  HideSystemMenuButtons();
  HideMainMenuButtons();
  HidePatternControlMenuButtons();
  HideOrreryMenuButtons();
  gCore2ScreenRenderer.SetEnabled(false);
  SetCore2ScreenBrightness(0);
  TouchButtonManager::Get()->Redraw();
}

void PatternControlButtonPressed(Player& player) {
  gScreenMode = ScreenMode::kPatternControlMenu;
  gLastScreenInteractionTime = TimeMicros();
  HideMainMenuButtons();
  gCore2ScreenRenderer.SetEnabled(false);
  TouchButtonManager::Get()->Redraw();
  DrawPatternControlMenuButtons();
  gPatternControlMenu.Draw();
}

void OrreryButtonPressed(Player& player) {
  gScreenMode = ScreenMode::kOrreryMenu;
  gLastScreenInteractionTime = TimeMicros();
  HideMainMenuButtons();
  gCore2ScreenRenderer.SetEnabled(false);
  TouchButtonManager::Get()->Redraw();
  DrawOrreryMenuButtons();
  gOrreryMenu.Draw();
}

void ConfirmButtonPressed(Player& player) {
  gLastScreenInteractionTime = TimeMicros();
  if (gScreenMode == ScreenMode::kPatternControlMenu) {
    if (gPatternControlMenu.ConfirmPressed(player)) {
      HidePatternControlMenuButtons();
      StartMainMenu(player);
    }
  } else if (gScreenMode == ScreenMode::kOrreryMenu) {
    if (gOrreryMenu.ConfirmPressed(player)) {
      HideOrreryMenuButtons();
      StartMainMenu(player);
    }
  }
}

Core2AwsUi::Core2AwsUi(Player& player)
    : Esp32Ui(player), ledBrightness_(kInitialLedBrightness), onBrightness_(kDefaultOnBrightness) {}

void Core2AwsUi::InitialSetup() {
  player_.SetBrightness(ledBrightness_);
  player_.SetOrrerySceneIdWatcher(this);
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
  HideOrreryMenuButtons();
  HideSystemMenuButtons();
  gUnlock1Button->Hide();
  gUnlock2Button->Hide();
  gPatternControlButton->SetCustomPaintFunction(DrawPatternControlButton);
  gOrreryButton->SetCustomPaintFunction(DrawOrreryButton);
  gSystemButton->SetCustomPaintFunction(DrawSystemButton);
  gConfirmButton->SetCustomPaintFunction(DrawConfirmButton);
  player_.AddStrand(kCore2ScreenPixels, gCore2ScreenRenderer);
  SetDefaultPrecedence(player_);
}

void Core2AwsUi::FinalSetup() {
  TouchButtonManager::Get()->MaybePaint();
  gCurrentPatternName = player_.CurrentEffectName();
  if (gScreenMode == ScreenMode::kMainMenu) {
    StartMainMenu(player_);
  } else {
    HideMainMenuButtons();
    gCore2ScreenRenderer.SetEnabled(false);
  }
}

void Core2AwsUi::DrawSystemTextLines() {
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
  snprintf(line, sizeof(line) - 1, "BLE: %s", Esp32BleNetwork::Get()->GetStatusStr().c_str());
  DrawSystemTextLine(i++, line);
#if JL_WIFI
  // Wi-Fi.
  snprintf(line, sizeof(line) - 1, "Wi-Fi: %s", WiFiNetwork::Get()->GetStatusStr().c_str());
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

void Core2AwsUi::RunLoop() {
  Microseconds currentTime = TimeMicros();
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
    jll_info("background pressed x=%d y=%d, free RAM %u/%u free PSRAM %u/%u", px, py,
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
          jll_info("starting unlock sequence from button press");
          gScreenMode = ScreenMode::kLocked1;
          gUnlock2Button->Hide();
          gUnlock1Button->Draw();
#else   // JL_BUTTON_LOCK
          jll_info("unlocking from button press");
          StartMainMenu(player_);
          gLastScreenInteractionTime = currentTime;
#endif  // JL_BUTTON_LOCK
        } break;
        case ScreenMode::kMainMenu: {
          if (px < 160 && py < 120) {
            gScreenMode = ScreenMode::kFullScreenPattern;
            jll_info("pattern screen pressed");
            HideMainMenuButtons();
            TouchButtonManager::Get()->Redraw();
            gCore2ScreenRenderer.SetFullScreen(true);
            gLastScreenInteractionTime = currentTime;
          }
        } break;
        case ScreenMode::kFullScreenPattern: {
          jll_info("full screen pattern pressed");
          gCore2ScreenRenderer.SetFullScreen(false);
          StartMainMenu(player_);
          gLastScreenInteractionTime = currentTime;
        } break;
        case ScreenMode::kPatternControlMenu: {
        } break;
        case ScreenMode::kOrreryMenu: {
        } break;
        case ScreenMode::kSystemMenu: {
        } break;
        case ScreenMode::kLocked1: {
          jll_info("ignoring background press during unlock1");
        } break;
        case ScreenMode::kLocked2: {
          jll_info("locking screen due to background press while unlocking");
          LockScreen();
        } break;
      }
    }
  } else {
    TouchButtonManager::Get()->HandleIdle();
  }
  if (gNextButton->JustReleased()) {
    jll_info("next pressed");
    if (gScreenMode == ScreenMode::kMainMenu) {
      gLastScreenInteractionTime = currentTime;
      player_.Next();
    }
  }
  if (gLoopButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kMainMenu) {
      jll_info("loop pressed");
      gLastScreenInteractionTime = currentTime;
      if (player_.isLooping()) {
        player_.StopLooping();
        gLoopButton->SetLabelText("Loop");
        gLoopButton->SetHighlight(false);
      } else {
        player_.LoopOne();
        gLoopButton->SetLabelText("Looping");
        gLoopButton->SetHighlight(true);
      }
      gLoopButton->Draw(/*force=*/true);
    } else {
      jll_info("ignoring loop pressed");
    }
  }
  if (gPatternControlButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kMainMenu) {
      jll_info("pattern control button pressed");
      PatternControlButtonPressed(player_);
    } else {
      jll_info("ignoring pattern control button pressed");
    }
  }
  if (gOrreryButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kMainMenu) {
      jll_info("orrery button pressed");
      OrreryButtonPressed(player_);
    } else {
      jll_info("ignoring orrery button pressed");
    }
  }
  if (gSystemButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kMainMenu) {
      jll_info("system button pressed");
      gScreenMode = ScreenMode::kSystemMenu;
      gLastScreenInteractionTime = currentTime;
      HideMainMenuButtons();
      gCore2ScreenRenderer.SetEnabled(false);
      TouchButtonManager::Get()->Redraw();
      DrawSystemMenuButtons();
      DrawSystemTextLines();
    } else {
      jll_info("ignoring system button pressed");
    }
  }
  if (gBackButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kSystemMenu ||
        (gScreenMode == ScreenMode::kPatternControlMenu && gPatternControlMenu.BackPressed()) ||
        gScreenMode == ScreenMode::kOrreryMenu) {
      jll_info("back button pressed");
      gLastScreenInteractionTime = currentTime;
      HidePatternControlMenuButtons();
      HideOrreryMenuButtons();
      HideSystemMenuButtons();
      StartMainMenu(player_);
    } else {
      jll_info("ignoring back button pressed");
    }
  }
  if (gDownButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kPatternControlMenu) {
      jll_info("down button pressed");
      gLastScreenInteractionTime = currentTime;
      gPatternControlMenu.DownPressed();
    } else if (gScreenMode == ScreenMode::kOrreryMenu) {
      jll_info("orrery down button pressed");
      gLastScreenInteractionTime = currentTime;
      gOrreryMenu.DownPressed();
    } else {
      jll_info("ignoring down button pressed");
    }
  }
  if (gUpButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kPatternControlMenu) {
      jll_info("up button pressed");
      gLastScreenInteractionTime = currentTime;
      gPatternControlMenu.UpPressed();
    } else if (gScreenMode == ScreenMode::kOrreryMenu) {
      jll_info("orrery up button pressed");
      gLastScreenInteractionTime = currentTime;
      gOrreryMenu.UpPressed();
    } else {
      jll_info("ignoring up button pressed");
    }
  }
  if (gOverrideButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kPatternControlMenu) {
      jll_info("override button pressed");
      gLastScreenInteractionTime = currentTime;
      gPatternControlMenu.OverridePressed(player_);
    } else {
      jll_info("ignoring override button pressed");
    }
  }
  if (gConfirmButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kPatternControlMenu || gScreenMode == ScreenMode::kOrreryMenu) {
      jll_info("confirm button pressed");
      ConfirmButtonPressed(player_);
    } else {
      jll_info("ignoring confirm button pressed");
    }
  }
  if (gLockButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kSystemMenu) {
      jll_info("lock button pressed");
      LockScreen();
    } else {
      jll_info("ignoring lock button pressed");
    }
  }
  if (gShutdownButton->JustReleased()) {
    if (gScreenMode == ScreenMode::kSystemMenu) {
      jll_info("shutdown button pressed");
      CorePowerOff();
    } else {
      jll_info("ignoring shutdown button pressed");
    }
  }
  if (gUnlock2Button->JustReleased()) {
    gLastScreenInteractionTime = currentTime;
    if (gScreenMode == ScreenMode::kLocked2) {
      jll_info("unlock2 button pressed");
      gUnlock2Button->Hide();
      StartMainMenu(player_);
    } else if (gScreenMode == ScreenMode::kMainMenu) {
      jll_info("unlock2 button unexpectedly pressed in main menu, treating as pattern control button");
      PatternControlButtonPressed(player_);
    } else if (gScreenMode == ScreenMode::kPatternControlMenu) {
      jll_info("unlock2 button unexpectedly pressed in pattern control menu, treating as confirm button");
      ConfirmButtonPressed(player_);
    } else if (gScreenMode == ScreenMode::kSystemMenu) {
      jll_info("unlock2 button unexpectedly pressed in system menu, treating as shutdown button");
      CorePowerOff();
    } else {
      jll_info("ignoring unlock2 button pressed");
    }
  }
  if (gUnlock1Button->JustReleased()) {
    gLastScreenInteractionTime = currentTime;
    if (gScreenMode == ScreenMode::kLocked1) {
      jll_info("unlock1 button pressed");
      gScreenMode = ScreenMode::kLocked2;
      gUnlock1Button->Hide();
      TouchButtonManager::Get()->Redraw();
      gUnlock2Button->Draw();
    } else if (gScreenMode == ScreenMode::kMainMenu) {
      jll_info("unlock1 button unexpectedly pressed in main menu, treating as next button");
      player_.Next();
    } else {
      jll_info("ignoring unlock1 button pressed");
    }
  }
  if (gLedPlusButton->JustReleased()) {
    jll_info("gLedPlusButton button pressed");
    if (ledBrightness_ < 255 && gScreenMode == ScreenMode::kSystemMenu) {
      ledBrightness_++;
      jll_info("setting LED brightness to %u", ledBrightness_);
      player_.SetBrightness(ledBrightness_);
      DrawSystemTextLines();
    }
  }
  if (gLedMinusButton->JustReleased()) {
    jll_info("gLedMinusButton button pressed");
    if (ledBrightness_ > 0 && gScreenMode == ScreenMode::kSystemMenu) {
      ledBrightness_--;
      jll_info("setting LED brightness to %u", ledBrightness_);
      player_.SetBrightness(ledBrightness_);
      DrawSystemTextLines();
    }
  }
  if (gScreenPlusButton->JustReleased()) {
    jll_info("gScreenPlusButton button pressed");
    if (onBrightness_ < kMaxOnBrightness && gScreenMode == ScreenMode::kSystemMenu) {
      onBrightness_++;
      SetCore2ScreenBrightness(onBrightness_);
      DrawSystemTextLines();
    }
  }
  if (gScreenMinusButton->JustReleased()) {
    jll_info("gScreenMinusButton button pressed");
    if (onBrightness_ > kMinOnBrightness && gScreenMode == ScreenMode::kSystemMenu) {
      onBrightness_--;
      SetCore2ScreenBrightness(onBrightness_);
      DrawSystemTextLines();
    }
  }

  std::string patternName = player_.CurrentEffectName();
  if (patternName != gCurrentPatternName) {
    gCurrentPatternName = patternName;
    if (gScreenMode == ScreenMode::kMainMenu) {
      jll_info("drawing new pattern name in pattern control button");
      gPatternControlButton->Draw(/*force=*/true);
    }
  }
#if JL_BUTTON_LOCK
  if (gLastScreenInteractionTime) {
    Microseconds lockTime = kLockDelay;
    if (gScreenMode == ScreenMode::kLocked1 || gScreenMode == ScreenMode::kLocked2) { lockTime = kUnlockingTime; }
    if (currentTime - *gLastScreenInteractionTime > lockTime) {
      jll_info("Locking screen due to inactivity");
      LockScreen();
    }
  }
#endif  // JL_BUTTON_LOCK
}

void Core2AwsUi::OnOrrerySceneId(std::optional<OrrerySceneId> orrerySceneId) {
  if (!orrerySceneId) { return; }
  OrreryScene scene = static_cast<OrreryScene>(*orrerySceneId);
  if (scene == gOrreryScene) { return; }
  jll_info("Received new orrery scene %d", static_cast<int>(scene));
  gOrreryScene = scene;
  if (gScreenMode == ScreenMode::kMainMenu) { gOrreryButton->Draw(/*force=*/true); }
}

#endif  // CORE2AWS_LCD_ENABLED

}  // namespace jazzlights

#endif  // CORE2AWS

/*

Note that Core2 and Core2AWS use the same pins. Similarly, CoreS3-SE and CoreS3 Lite use the pins from CoreS3.

Bus | Core | Core2| Core | CoreExt | Core
Pos | Basic| AWS  |  S3  |  Port   | Func
    | Pin  | Pin  | Pin  |         |
----+------+------+------+---------+--------
  1 | GND  | GND  | GND  |         |
  2 | G35  | G35  | G10  |  D2.1   | ADC (unusable for GPIO on Core2Aws)
  3 | GND  | GND  | GND  |         |
  4 | G36  | G36  | G8   |  B1     | ADC/PB_IN (unusable for GPIO on Core2Aws)
  5 | GND  | GND  | GND  |         |
  6 | RST  | RST  | RST  |         | EN
  7 | G23  | G23  | G37  |         | SPI_MOSI
  8 | G25  | G25  | G5   |  E2.2   | DAC/SPK/GPIO/LED_Core2AwsBase
  9 | G19  | G38  | G35  |         | SPI_MISO
 10 | G26  | G26  | G9   |  B2     | DAC/PB_OUT/LAN12_CS
 11 | G18  | G18  | G36  |         | SPI_SCK
 12 | 3V3  | 3V3  | 3V3  |         |
 13 | G3   | G3   | G44  |  D1.3   | RXD0 (USB_CP2104 serial)
 14 | G1   | G1   | G43  |  D2.3   | TXD0 (USB_CP2104 serial)
 15 | G16  | G13  | G18  |  C1     | RXD2/PC_RX
 16 | G17  | G14  | G17  |  C2     | TXD2/PC_TX
 17 | G21  | G21  | G12  |  D2.2   | Int_SDA
 18 | G22  | G22  | G11  |  D1.2   | Int_SCL
 19 | G2   | G32  | G2   |  A2     | PortA_SDA
 20 | G5   | G33  | G1   |  A1     | PortA_SCL/LAN12_RS485_TX/LAN13_CSN?
 21 | G12  | G27  | G6   |  E1.1   | GPIO
 22 | G13  | G19  | G7   |  E2.1   | GPIO/LAN12_RST/LAN13_RSTN?
 23 | G15  | G2   | G13  |E1.2/E2.3| I2S_DOUT/LAN12_RS485_RX/LAN13_CSN?
 24 | G0   | G0   | G0   |  E1.3   | I2S_LRCK/LAN13_RSTN?
 25 | HPWR |      | HVIN |         | HPWR
 26 | G34  | G34  | G14  |  D1.1   | I2S_DIN/LAN12_INT/LAN13_INTN? (unusable for GPIO on Core2Aws)
 27 | HPWR |      | HVIN |         | HPWR
 28 | 5V   | 5V   | 5V   |         |
 29 | HPWR |      | HVIN |         | HPWR
 30 | BAT  | BAT  | BAT  |         | BAT

  Note that the original pinout docs for the Core Base LAN v1.2 used the GPIO numbers from the Core Basic instead of the
  Core2. See mapping of differences below.

  GPIO pins on Core2AWS with Base LAN v1.2 module:
   0 = NS4168-LRCK = PortE1.3
   1 = USB_CP2104 RXD = PortD1.3
   2 = NS4168-DATA = PortE1.2 = PortE2.3
   3 = USB_CP2104 TXD = PortD2.3
   4 = TF CS
   5 = LCD CS
  13 = PortC1
  14 = PortC2
  15 = LCD DC
  18 = W5500 SCLK = LCD SCLK = TF SCLK
  19 = W5500 RST = PortE2.1
  21 = internal I2C SDA = PortD2.2
  22 = internal I2C SCL = PortD1.2
  23 = W5500 MOSI = LCD MOSI = TF MOSI
  25 = LED (only when AWS expansion is used) = PortE2.2
  26 = W5500 CS = PortB2
  27 = PortE1.1
  32 = PortA2
  33 = PortA1
  34 = W5500 INTn = PortD1.1
  35 = PortD2.1
  36 = PortB1
  38 = LCD MISO = TF MISO = W5500 MISO

  Good: A1,A2,B1,C1,C2,D2.1,E1.1,E2.2 -- total of 8 pins
  Taken by W5500 ethernet: B2,D1

  https://docs.m5stack.com/en/core/basic
  https://docs.m5stack.com/en/core/core2_for_aws
  https://docs.m5stack.com/en/core/CoreS3
  https://docs.m5stack.com/en/module/extport_for_core2
  https://docs.m5stack.com/en/base/lan_v12
  https://docs.m5stack.com/en/module/LAN%20Module%2013.2
*/
