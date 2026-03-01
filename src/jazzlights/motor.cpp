#include "jazzlights/motor.h"

#if JL_MOTOR

#include <driver/gpio.h>
#include <driver/mcpwm_cmpr.h>
#include <driver/mcpwm_gen.h>
#include <driver/mcpwm_oper.h>
#include <driver/mcpwm_timer.h>

#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

StepperMotor::StepperMotor(int enablePin, int directionPin, int stepPin)
    : enablePin_(static_cast<gpio_num_t>(enablePin)),
      directionPin_(static_cast<gpio_num_t>(directionPin)),
      stepPin_(static_cast<gpio_num_t>(stepPin)) {}

void StepperMotor::SetSpeed(int32_t frequencyHz) {
  jll_info("%u Setting speed to %lldHz", timeMillis(), static_cast<int64_t>(frequencyHz));
  if (Setup(frequencyHz)) { return; }
  uint32_t halfPeriod;
  if (frequencyHz > 0) {
    SetDirection(true);
    halfPeriod = kResolution / (2 * frequencyHz);
  } else if (frequencyHz < 0) {
    SetDirection(false);
    halfPeriod = kResolution / (2 * -frequencyHz);
  } else {
    SetEnabled(false);
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer_, MCPWM_TIMER_STOP_EMPTY));
    return;
  }
  SetEnabled(true);
  ESP_ERROR_CHECK(mcpwm_timer_set_period(timer_, 2 * halfPeriod));
  ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator_, halfPeriod));
  ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer_, MCPWM_TIMER_START_NO_STOP));
}

bool StepperMotor::Setup(int32_t frequencyHz) {
  if (isSetup_) { return false; }
  if (frequencyHz == 0) { return true; }  // Don't set up yet.
  jll_info("%u Setting up motor at %lldHz", timeMillis(), static_cast<int64_t>(frequencyHz));
  isSetup_ = true;
  gpio_config_t directionConf = {
      .pin_bit_mask = (1ULL << directionPin_),
      .mode = GPIO_MODE_OUTPUT,
  };
  ESP_ERROR_CHECK(gpio_config(&directionConf));

  gpio_config_t enableConf = {
      .pin_bit_mask = (1ULL << enablePin_),
      .mode = GPIO_MODE_OUTPUT,
  };
  ESP_ERROR_CHECK(gpio_config(&enableConf));
  SetEnabled(true);

  uint32_t halfPeriod;
  if (frequencyHz > 0) {
    SetDirection(true);
    halfPeriod = kResolution / (2 * frequencyHz);
  } else if (frequencyHz < 0) {
    SetDirection(false);
    halfPeriod = kResolution / (2 * -frequencyHz);
  }

  constexpr int kGroupId = 0;
  mcpwm_timer_config_t timerConfig = {
      .group_id = kGroupId,
      .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
      .resolution_hz = kResolution,
      .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
      .period_ticks = 2 * halfPeriod,
      .intr_priority = 0,
      .flags = {},
  };
  ESP_ERROR_CHECK(mcpwm_new_timer(&timerConfig, &timer_));

  mcpwm_operator_config_t operatorConfig = {
      .group_id = kGroupId,
      .intr_priority = 0,
      .flags = {},
  };
  ESP_ERROR_CHECK(mcpwm_new_operator(&operatorConfig, &oper_));
  ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper_, timer_));

  mcpwm_generator_config_t genConfig = {
      .gen_gpio_num = stepPin_,
      .flags = {},
  };
  ESP_ERROR_CHECK(mcpwm_new_generator(oper_, &genConfig, &generator_));

  mcpwm_comparator_config_t comparator_config = {
      .intr_priority = 0,
      .flags = {.update_cmp_on_tez = true},
  };
  ESP_ERROR_CHECK(mcpwm_new_comparator(oper_, &comparator_config, &comparator_));
  ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator_, halfPeriod));
  ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(
      generator_,
      MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
  ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(
      generator_, MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator_, MCPWM_GEN_ACTION_LOW)));

  ESP_ERROR_CHECK(mcpwm_timer_enable(timer_));
  ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer_, MCPWM_TIMER_START_NO_STOP));
  return true;
}

void StepperMotor::SetEnabled(bool enabled) {
  if (enablePin_ == GPIO_NUM_NC) { return; }
  if (enabled == lastEnabled_) { return; }
  ESP_ERROR_CHECK(gpio_set_level(enablePin_, (enabled ? 0 : 1)));
  lastEnabled_ = enabled;
}

void StepperMotor::SetDirection(bool direction) {
  if (directionPin_ == GPIO_NUM_NC) { return; }
  if (direction == lastDirection_) { return; }
  ESP_ERROR_CHECK(gpio_set_level(directionPin_, (direction ? 1 : 0)));
  lastDirection_ = direction;
}

StepperMotor* GetMainStepperMotor() {
#if JL_IS_CONTROLLER(ATOM_LITE) || JL_IS_CONTROLLER(ATOM_MATRIX) || JL_IS_CONTROLLER(ATOM_S3) || \
    JL_IS_CONTROLLER(CORES3) || JL_IS_CONTROLLER(CORE2AWS)
  static constexpr int kEnablePin = kPinC2;
  static constexpr int kDirectionPin = kPinC1;
  static constexpr int kStepPin = kPinB2;
#elif JL_IS_CONTROLLER(M5STAMP_S3)
  static constexpr int kEnablePin = 3;
  static constexpr int kDirectionPin = 4;
  static constexpr int kStepPin = 2;
#else
#error "unknown controller for motor"
#endif
  static StepperMotor sStepper(kEnablePin, kDirectionPin, kStepPin);
  return &sStepper;
}

void StepperMotorTestRunLoop(Milliseconds currentTime) {
  static uint8_t sEpoch = 0;
  static constexpr int32_t kSpeeds[] = {3000, 0, -3000, 0};
  static constexpr Milliseconds kDurations[] = {1000, 5000, 1000, 5000};
  static_assert(sizeof(kSpeeds) / sizeof(kSpeeds[0]) == sizeof(kDurations) / sizeof(kDurations[0]), "bad sizes");
  static Milliseconds sTimeOfLastChange = -1;
  if (sTimeOfLastChange < 0 || currentTime - sTimeOfLastChange > kDurations[sEpoch]) {
    GetMainStepperMotor()->SetSpeed(kSpeeds[sEpoch]);
    sTimeOfLastChange = currentTime;
    sEpoch++;
    if (sEpoch >= (sizeof(kSpeeds) / sizeof(kSpeeds[0]))) { sEpoch = 0; }
  }
}

}  // namespace jazzlights

#endif  // JL_MOTOR