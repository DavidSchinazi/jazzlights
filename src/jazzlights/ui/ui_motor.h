#ifndef JL_UI_UI_MOTOR_H
#define JL_UI_UI_MOTOR_H

#include "jazzlights/config.h"
#include "jazzlights/motor.h"

#ifndef JL_UI_MOTOR
#if JL_MOTOR && defined(ESP32) && (JL_IS_CONTROLLER(CORE2AWS) || JL_IS_CONTROLLER(CORES3))
#define JL_UI_MOTOR 1
#else  // CORE2AWS || CORES3
#define JL_UI_MOTOR 0
#endif  // CORE2AWS || CORES3
#endif  // JL_UI_MOTOR

#if JL_UI_MOTOR

#include <M5Unified.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "jazzlights/ui/touch_button.h"
#include "jazzlights/ui/ui.h"

namespace jazzlights {

class CoreMotorUi : public Esp32Ui {
 public:
  explicit CoreMotorUi(Player& player);
  // From Esp32Ui.
  void InitialSetup() override;
  void FinalSetup() override;
  void RunLoop(Milliseconds currentTime) override;

 private:
  void SetMotorSpeed();
  void UpdateMotorSpeedButton();
  void DrawSpeedDisplayButton(TouchButton* button, int outline, int fill, int textColor);
  TouchButton* planetButton_ = nullptr;
  TouchButton* motorEnableButton_ = nullptr;
  TouchButton* motorDirectionButton_ = nullptr;
  TouchButton* motorSpeedButton_ = nullptr;
  TouchButton* backButton_ = nullptr;
  TouchButton* speedDisplayButton_ = nullptr;
  TouchButton* keypadButtons_[10] = {};
  TouchButton* clearButton_ = nullptr;
  TouchButton* confirmButton_ = nullptr;
  int32_t motorFrequencyHz_ = 1000;
  bool motorEnabled_ = false;
  bool motorDirectionForward_ = true;
  bool keypadActive_ = false;
  int32_t keypadValue_ = 0;
};

}  // namespace jazzlights

#endif  // JL_UI_MOTOR
#endif  // JL_UI_UI_MOTOR_H
