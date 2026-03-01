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
  TouchButtonManager::Get()->MaybePaint();
}

void CoreMotorUi::SetMotorSpeed() {
  int32_t motorFrequencyHz = motorFrequencyHz_;
  if (!motorEnabled_) { motorFrequencyHz = 0; }
  if (!motorDirectionForward_) { motorFrequencyHz = -motorFrequencyHz; }
  GetMainStepperMotor()->SetSpeed(motorFrequencyHz);
}

}  // namespace jazzlights

#endif  // JL_UI_MOTOR
