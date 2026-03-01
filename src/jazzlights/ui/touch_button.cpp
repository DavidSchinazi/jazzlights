#include "jazzlights/ui/touch_button.h"

#if JL_TOUCH_SCREEN

#include <M5Unified.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "jazzlights/util/log.h"

namespace jazzlights {

void TouchButton::Setup(int16_t x, int16_t y, uint16_t w, uint16_t h, const char* label) {
  x_ = x;
  y_ = y;
  w_ = w;
  h_ = h;
  lgfxButton_.initButtonUL(&M5.Lcd, x, y, w, h,
                           /*outline=*/TFT_WHITE, /*fill=*/TFT_BLACK, /*textcolor=*/TFT_WHITE, "");
  SetLabelText(label);
}

void TouchButton::SetLabelText(const char* label) {
  label_ = std::string(label);
  Paint();
}

void TouchButton::SetLabelDatum(int16_t x_delta, int16_t y_delta, lgfx::textdatum::textdatum_t datum) {
  lgfxButton_.setLabelDatum(x_delta, y_delta, datum);
  dx_ = x_delta;
  dy_ = y_delta;
  Paint();
}

void TouchButton::Hide() {
  hidden_ = true;
  MaybePaint();
}

void TouchButton::Draw(bool force) {
  hidden_ = false;
  MaybePaint(force);
}

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

bool TouchButton::IsPressed() const {
  if (hidden_) { return false; }
  return lgfxButton_.isPressed();
}

bool TouchButton::JustPressed() const {
  if (hidden_) { return false; }
  return lgfxButton_.justPressed();
}

bool TouchButton::JustReleased() const {
  if (hidden_) { return false; }
  return lgfxButton_.justReleased();
}

bool TouchButton::HandlePress(int16_t x, int16_t y) {
  bool ret = false;
  if (!hidden_) {
    ret = lgfxButton_.contains(x, y);
    lgfxButton_.press(ret);
  }
  MaybePaint();
  return ret;
}

void TouchButton::HandleIdle() {
  lgfxButton_.press(false);
  MaybePaint();
}

void TouchButton::MaybePaint(bool force) {
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

void TouchButton::PaintRectangle(int fill, int outline) {
  uint16_t cornerRadius = std::min<uint16_t>(w_, h_) / 4;
  M5.Lcd.fillRoundRect(x_ + 1, y_ + 1, w_ - 2, h_ - 2, cornerRadius, fill);
  M5.Lcd.drawRoundRect(x_ + 1, y_ + 1, w_ - 2, h_ - 2, cornerRadius, outline);
}

void TouchButton::PaintText(int textColor, int fill, const char* label) {
  const char* printLabel = (label != nullptr ? label : label_.c_str());
  M5.Lcd.setTextColor(textColor, fill);
  M5.Lcd.drawString(printLabel, x_ + (w_ / 2) + dx_, y_ + (h_ / 2) + dy_);
}

TouchButton* TouchButtonManager::AddButton(int16_t x, int16_t y, uint16_t w, uint16_t h, const char* label) {
  buttons_.push_back(std::unique_ptr<TouchButton>(new TouchButton()));
  std::unique_ptr<TouchButton>& button = buttons_.back();
  button->Setup(x, y, w, h, label);
  return button.get();
}

// static
TouchButtonManager* TouchButtonManager::Get() {
  static TouchButtonManager sTouchButtonManager;
  return &sTouchButtonManager;
}

bool TouchButtonManager::HandlePress(int16_t x, int16_t y) {
  bool ret = false;
  for (auto& button : buttons_) {
    if (button->IsHidden()) {
      if (button->HandlePress(x, y)) { ret = true; }
    }
  }
  for (auto& button : buttons_) {
    if (!button->IsHidden()) {
      if (button->HandlePress(x, y)) { ret = true; }
    }
  }
  return ret;
}

void TouchButtonManager::HandleIdle() {
  for (auto& button : buttons_) {
    if (button->IsHidden()) { button->HandleIdle(); }
  }
  for (auto& button : buttons_) {
    if (!button->IsHidden()) { button->HandleIdle(); }
  }
}

void TouchButtonManager::MaybePaint() {
  for (auto& button : buttons_) {
    if (button->IsHidden()) { button->MaybePaint(); }
  }
  for (auto& button : buttons_) {
    if (!button->IsHidden()) { button->MaybePaint(); }
  }
}

void TouchButtonManager::RedrawRightHalf() {
  M5.Lcd.fillRect(/*x=*/165, /*y=*/0, /*w=*/155, /*h=*/240, BLACK);
  for (auto& button : buttons_) {
    if (button->IsHidden()) { button->Paint(); }
  }
  for (auto& button : buttons_) {
    if (!button->IsHidden()) { button->Paint(); }
  }
}

void TouchButtonManager::Redraw() {
  M5.Lcd.fillScreen(BLACK);
  for (auto& button : buttons_) {
    if (button->IsHidden()) { button->Paint(); }
  }
  for (auto& button : buttons_) {
    if (!button->IsHidden()) { button->Paint(); }
  }
}

}  // namespace jazzlights

#endif  // JL_TOUCH_SCREEN
