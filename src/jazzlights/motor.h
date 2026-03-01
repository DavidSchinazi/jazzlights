#ifndef JL_MOTOR_H
#define JL_MOTOR_H

#include "jazzlights/config.h"

#if JL_MOTOR

#include <driver/gpio.h>
#include <driver/mcpwm_cmpr.h>
#include <driver/mcpwm_gen.h>
#include <driver/mcpwm_oper.h>
#include <driver/mcpwm_timer.h>

#include "jazzlights/util/time.h"

namespace jazzlights {

class StepperMotor {
 public:
  StepperMotor(int enablePin, int directionPin, int stepPin);
  // Disallow copy and move.
  StepperMotor(const StepperMotor&) = delete;
  StepperMotor(StepperMotor&&) = delete;
  StepperMotor& operator=(const StepperMotor&) = delete;
  StepperMotor& operator=(StepperMotor&&) = delete;

  void SetSpeed(int32_t frequencyHz);

 private:
  bool Setup(int32_t frequencyHz);
  void SetEnabled(bool enabled);
  void SetDirection(bool direction);

  const gpio_num_t enablePin_;
  const gpio_num_t directionPin_;
  const gpio_num_t stepPin_;
  bool lastEnabled_ = false;
  bool lastDirection_ = false;
  bool isSetup_ = false;
  mcpwm_timer_handle_t timer_ = nullptr;
  mcpwm_oper_handle_t oper_ = nullptr;
  mcpwm_gen_handle_t generator_ = nullptr;
  mcpwm_cmpr_handle_t comparator_ = nullptr;
  static constexpr uint32_t kResolution = 1000000;
};

StepperMotor* GetMainStepperMotor();
void StepperMotorTestRunLoop(Milliseconds currentTime);

}  // namespace jazzlights

#endif  // JL_MOTOR
#endif  // JL_MOTOR_H
