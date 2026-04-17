#ifndef JL_UI_UI_ORRERY_LEADER_H
#define JL_UI_UI_ORRERY_LEADER_H

#include "jazzlights/config.h"

#if JL_IS_CONFIG(ORRERY_LEADER) && defined(ESP32) && (JL_IS_CONTROLLER(CORE2AWS) || JL_IS_CONTROLLER(CORES3))

#include <M5Unified.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "jazzlights/orrery_common.h"
#include "jazzlights/ui/touch_button.h"
#include "jazzlights/ui/ui.h"

namespace jazzlights {

class OrreryLeaderUi : public Esp32Ui {
 public:
  explicit OrreryLeaderUi(Player& player);
  // From Esp32Ui.
  void InitialSetup() override;
  void FinalSetup() override;
  void RunLoop(Milliseconds currentTime) override;

 private:
  void SetMotorSpeed();
  void UpdateMotorSpeedButton();
  void UpdateLedBrightnessButton();
  void UpdatePlanetButton();
  void UpdatePlanetHalfButton();
  void UpdatePlanetOffsetButton();
  void UpdateHallSensorButton();
  void UpdateHallSensorSubmenu();
  void UpdatePlanetPattern();
  void DrawSpeedDisplayButton(TouchButton* button, int outline, int fill, int textColor);
  TouchButton* planetButton_ = nullptr;
  TouchButton* ledBrightnessButton_ = nullptr;
  TouchButton* hallSensorButton_ = nullptr;
  TouchButton* motorEnableButton_ = nullptr;
  TouchButton* motorDirectionButton_ = nullptr;
  TouchButton* motorSpeedButton_ = nullptr;
  TouchButton* planetHalfButton_ = nullptr;
  TouchButton* planetOffsetButton_ = nullptr;
  TouchButton* backButton_ = nullptr;
  TouchButton* speedDisplayButton_ = nullptr;
  TouchButton* keypadButtons_[10] = {};
  TouchButton* clearButton_ = nullptr;
  TouchButton* confirmButton_ = nullptr;
  TouchButton* planetSelectButtons_[kNumPlanets] = {};
  TouchButton* planetBackButton_ = nullptr;
  TouchButton* hallSensorBackButton_ = nullptr;
  TouchButton* hallSensorInfoButtons_[4] = {};
  int32_t motorFrequencyHz_ = kDefaultPlanetSpeed;
  uint8_t ledBrightness_ = kDefaultPlanetBrightness;
  bool planetHalf_ = false;
  uint8_t planetOffset_ = 0;
  bool motorEnabled_ = false;
  bool motorDirectionForward_ = true;
  bool keypadActive_ = false;
  bool editingBrightness_ = false;
  bool editingOffset_ = false;
  bool planetSubmenuActive_ = false;
  bool hallSensorSubmenuActive_ = false;
  int32_t keypadValue_ = 0;
  Planet currentPlanet_ = Planet::Mercury;
};

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_LEADER) && ESP32 && (CORE2AWS || CORES3)
#endif  // JL_UI_UI_ORRERY_LEADER_H
