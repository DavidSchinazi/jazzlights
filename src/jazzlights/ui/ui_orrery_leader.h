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
#include "jazzlights/orrery_leader.h"
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
  void UpdateSceneButton();
  void UpdateLedBrightnessButton();
  void UpdatePlanetButton();
  void UpdatePlanetHalfButton();
  void UpdatePlanetOffsetButton();
  void UpdateHallSensorButton();
  void UpdateHallSensorSubmenu();
  void UpdatePlanetPattern();
  void UpdateLedMenuButton();
  void UpdateMotorMenuButton();
  void UpdateCalibrationMenuButton();
  void UpdateStatusMenuButton();
  void UpdateStatusSubmenu();
  void UpdateMotorDirectionButton();
  void UpdateMotorPositionButton();
  void UpdateMotorCurrentPositionButton();
  void UpdateMotorCurrentSpeedButton();
  void UpdateMotorCalibrationButton();
  void DrawSpeedDisplayButton(TouchButton* button, int outline, int fill, int textColor);
  void HideAll();
  void DrawMainMenu();
  void DrawLedMenu();
  void DrawMotorMenu();
  void DrawCalibrationMenu();
  void DrawStatusMenu();
  void DrawKeypad();
  void DrawPlanetMenu();
  void DrawSceneMenu();
  TouchButton* planetButton_ = nullptr;
  TouchButton* sceneButton_ = nullptr;
  TouchButton* ledMenuButton_ = nullptr;
  TouchButton* motorMenuButton_ = nullptr;
  TouchButton* calibrationMenuButton_ = nullptr;
  TouchButton* statusMenuButton_ = nullptr;
  TouchButton* ledBrightnessButton_ = nullptr;
  TouchButton* motorEnableButton_ = nullptr;
  TouchButton* motorDirectionButton_ = nullptr;
  TouchButton* motorPositionButton_ = nullptr;
  TouchButton* motorCurrentPositionButton_ = nullptr;
  TouchButton* motorCurrentSpeedButton_ = nullptr;
  TouchButton* motorCalibrationButton_ = nullptr;
  TouchButton* motorSpeedButton_ = nullptr;
  TouchButton* planetHalfButton_ = nullptr;
  TouchButton* planetOffsetButton_ = nullptr;
  TouchButton* keypadBackButton_ = nullptr;
  TouchButton* speedDisplayButton_ = nullptr;
  TouchButton* keypadButtons_[10] = {};
  TouchButton* clearButton_ = nullptr;
  TouchButton* confirmButton_ = nullptr;
  TouchButton* planetSelectButtons_[kNumPlanets + 1] = {};
  TouchButton* sceneSelectButtons_[14] = {};
  TouchButton* planetBackButton_ = nullptr;
  TouchButton* sceneBackButton_ = nullptr;
  TouchButton* ledBackButton_ = nullptr;
  TouchButton* motorBackButton_ = nullptr;
  TouchButton* hallSensorBackButton_ = nullptr;
  TouchButton* statusBackButton_ = nullptr;
  TouchButton* hallSensorInfoButtons_[4] = {};
  TouchButton* statusInfoButtons_[kNumPlanets] = {};
  int32_t motorMilliRpm_ = 0;
  uint8_t ledBrightness_ = kDefaultPlanetBrightness;
  enum class PlanetPatternMode {
    Full,
    Half,
    Hall,
  };
  PlanetPatternMode planetPatternMode_ = PlanetPatternMode::Full;
  OrreryScene lastScene_ = static_cast<OrreryScene>(-1);
  uint8_t planetOffset_ = 0;
  bool motorEnabled_ = false;
  bool motorForward_ = true;
  bool keypadActive_ = false;
  bool editingBrightness_ = false;
  bool editingOffset_ = false;
  bool editingPosition_ = false;
  bool ledSubmenuActive_ = false;
  bool motorSubmenuActive_ = false;
  bool planetSubmenuActive_ = false;
  bool sceneSubmenuActive_ = false;
  bool hallSensorSubmenuActive_ = false;
  bool statusSubmenuActive_ = false;
  int32_t keypadValue_ = 0;
  bool keypadHasValue_ = false;
  Planet currentPlanet_ = Planet::Mercury;
  std::optional<uint32_t> targetPosition_;
};

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_LEADER) && ESP32 && (CORE2AWS || CORES3)
#endif  // JL_UI_UI_ORRERY_LEADER_H
