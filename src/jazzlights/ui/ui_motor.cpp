#include "jazzlights/ui/ui_motor.h"

#if JL_UI_MOTOR

#include <M5Unified.h>

#include "jazzlights/motor.h"
#include "jazzlights/ui/touch_button.h"
#include "jazzlights/util/log.h"

namespace jazzlights {
namespace {
constexpr uint8_t kDefaultOnBrightness = 32;
}  // namespace

CoreMotorUi::CoreMotorUi(Player& player) : Esp32Ui(player) {}

void CoreMotorUi::InitialSetup() {  // 320w * 240h
  auto cfg = M5.config();
  cfg.serial_baudrate = 0;
  M5.begin(cfg);

  planetButton_ = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/0, /*w=*/160, /*h=*/120, "Planet: Local");
  motorEnableButton_ = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/0, /*w=*/160, /*h=*/60, "Motor Disabled");
  motorDirectionButton_ =
      TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/60, /*w=*/160, /*h=*/60, "Motor Forward");
  motorSpeedButton_ = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/120, /*w=*/160, /*h=*/60, "Motor Speed");
  UpdateMotorSpeedButton();

  const int w = 320 / 3;
  const int h = 240 / 4;
  for (int i = 1; i <= 9; i++) {
    char label[2] = {static_cast<char>('0' + i), '\0'};
    keypadButtons_[i] = TouchButtonManager::Get()->AddButton(((i - 1) % 3) * w, ((i - 1) / 3) * h, w, h, label);
    keypadButtons_[i]->Hide();
  }
  keypadButtons_[0] = TouchButtonManager::Get()->AddButton(w, 3 * h, w, h, "0");
  keypadButtons_[0]->Hide();
  clearButton_ = TouchButtonManager::Get()->AddButton(0, 3 * h, w, h, "Clear");
  clearButton_->Hide();
  confirmButton_ = TouchButtonManager::Get()->AddButton(2 * w, 3 * h, 320 - 2 * w, h, "Confirm");
  confirmButton_->Hide();

  M5.Display.fillScreen(BLACK);
  M5.Display.setBrightness(kDefaultOnBrightness);
  M5.Display.wakeup();
}

void CoreMotorUi::FinalSetup() {
  planetButton_->Draw();
  motorEnableButton_->Draw();
  motorDirectionButton_->Draw();
  motorSpeedButton_->Draw();
  TouchButtonManager::Get()->MaybePaint();
}

void CoreMotorUi::RunLoop(Milliseconds currentTime) {
  M5.update();
  auto touchDetail = M5.Touch.getDetail();
  if (touchDetail.isPressed()) {
    int16_t px = touchDetail.x;
    int16_t py = touchDetail.y;
    jll_info("%u button pressed x=%d y=%d", currentTime, px, py);
    bool buttonPressed = TouchButtonManager::Get()->HandlePress(px, py);
  } else {
    TouchButtonManager::Get()->HandleIdle();
  }

  if (keypadActive_) {
    for (int i = 0; i <= 9; i++) {
      if (keypadButtons_[i]->JustReleased()) {
        keypadValue_ = keypadValue_ * 10 + i;
        UpdateConfirmButton();
      }
    }
    if (clearButton_->JustReleased()) {
      keypadValue_ = 0;
      UpdateConfirmButton();
    }
    if (confirmButton_->JustReleased()) {
      motorFrequencyHz_ = keypadValue_;
      SetMotorSpeed();
      UpdateMotorSpeedButton();
      keypadActive_ = false;
      for (int i = 0; i <= 9; i++) { keypadButtons_[i]->Hide(); }
      clearButton_->Hide();
      confirmButton_->Hide();
      planetButton_->Draw();
      motorEnableButton_->Draw();
      motorDirectionButton_->Draw();
      motorSpeedButton_->Draw();
      TouchButtonManager::Get()->Redraw();
    }
  } else {
    if (motorEnableButton_->JustReleased()) {
      motorEnabled_ = !motorEnabled_;
      motorEnableButton_->SetLabelText(motorEnabled_ ? "Motor Enabled" : "Motor Disabled");
      SetMotorSpeed();
    }
    if (motorDirectionButton_->JustReleased()) {
      motorDirectionForward_ = !motorDirectionForward_;
      motorDirectionButton_->SetLabelText(motorDirectionForward_ ? "Motor Forward" : "Motor Reverse");
      SetMotorSpeed();
    }
    if (motorSpeedButton_->JustReleased()) {
      keypadActive_ = true;
      keypadValue_ = 0;
      planetButton_->Hide();
      motorEnableButton_->Hide();
      motorDirectionButton_->Hide();
      motorSpeedButton_->Hide();
      for (int i = 0; i <= 9; i++) { keypadButtons_[i]->Draw(); }
      clearButton_->Draw();
      confirmButton_->Draw();
      UpdateConfirmButton();
      TouchButtonManager::Get()->Redraw();
    }
  }
  TouchButtonManager::Get()->MaybePaint();
}

void CoreMotorUi::SetMotorSpeed() {
  int32_t motorFrequencyHz = motorFrequencyHz_;
  if (!motorEnabled_) { motorFrequencyHz = 0; }
  if (!motorDirectionForward_) { motorFrequencyHz = -motorFrequencyHz; }
  GetMainStepperMotor()->SetSpeed(motorFrequencyHz);
}

void CoreMotorUi::UpdateMotorSpeedButton() {
  char label[32];
  snprintf(label, sizeof(label), "Speed: %lld", static_cast<int64_t>(motorFrequencyHz_));
  motorSpeedButton_->SetLabelText(label);
}

void CoreMotorUi::UpdateConfirmButton() {
  char label[32];
  snprintf(label, sizeof(label), "OK: %lld", static_cast<int64_t>(keypadValue_));
  confirmButton_->SetLabelText(label);
}

}  // namespace jazzlights

#endif  // JL_UI_MOTOR
