#include "jazzlights/ui/ui_orrery_leader.h"

#if JL_IS_CONFIG(ORRERY_LEADER) && defined(ESP32) && (JL_IS_CONTROLLER(CORE2AWS) || JL_IS_CONTROLLER(CORES3))

#include <M5Unified.h>

#include <cstdlib>
#include <functional>

#include "jazzlights/orrery_leader.h"
#include "jazzlights/ui/touch_button.h"
#include "jazzlights/util/log.h"

namespace jazzlights {
namespace {
constexpr uint8_t kDefaultOnBrightness = 128;
}  // namespace

OrreryLeaderUi::OrreryLeaderUi(Player& player) : Esp32Ui(player) {}

void OrreryLeaderUi::InitialSetup() {  // 320w * 240h
  auto cfg = M5.config();
  cfg.serial_baudrate = 0;
  M5.begin(cfg);

  // Main menu buttons.
  planetButton_ = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/0, /*w=*/320, /*h=*/60, "");
  sceneButton_ = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/60, /*w=*/320, /*h=*/60, "");
  UpdateSceneButton();
  ledMenuButton_ = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/120, /*w=*/160, /*h=*/60, "LED");
  motorMenuButton_ = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/120, /*w=*/160, /*h=*/60, "Motor");
  calibrationMenuButton_ = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/180, /*w=*/106, /*h=*/60, "HS");
  statusMenuButton_ = TouchButtonManager::Get()->AddButton(/*x=*/106, /*y=*/180, /*w=*/107, /*h=*/60, "Status");
  speedMultiplierButton_ = TouchButtonManager::Get()->AddButton(/*x=*/213, /*y=*/180, /*w=*/107, /*h=*/60, "");
  UpdateSpeedMultiplierButton();

  // LED submenu buttons.
  ledBrightnessButton_ = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/0, /*w=*/320, /*h=*/60, "");
  ledBrightness_ = OrreryLeader::Get()->GetBrightness(currentPlanet_);
  UpdateLedBrightnessButton();
  planetHalfButton_ = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/60, /*w=*/320, /*h=*/60, "");
  UpdatePlanetHalfButton();
  planetOffsetButton_ = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/120, /*w=*/320, /*h=*/60, "");
  UpdatePlanetOffsetButton();
  ledBackButton_ = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/180, /*w=*/320, /*h=*/60, "Back");

  // Motor submenu buttons.
  int32_t speed = OrreryLeader::Get()->GetSpeed(currentPlanet_);
  if (speed == kOrrerySpeedDisable) {
    motorEnabled_ = false;
  } else {
    motorEnabled_ = true;
    motorForward_ = (speed >= 0);
    motorMilliRpm_ = std::abs(speed);
  }
  targetPosition_ = OrreryLeader::Get()->GetTargetPosition(currentPlanet_);
  motorEnableButton_ = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/0, /*w=*/160, /*h=*/48,
                                                            motorEnabled_ ? "Enabled" : "Disabled");
  motorDirectionButton_ = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/0, /*w=*/160, /*h=*/48, "");
  UpdateMotorDirectionButton();
  motorSpeedButton_ = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/48, /*w=*/160, /*h=*/48, "");
  UpdateMotorSpeedButton();
  motorCurrentSpeedButton_ = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/48, /*w=*/160, /*h=*/48, "");
  UpdateMotorCurrentSpeedButton();
  motorCurrentHzButton_ = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/96, /*w=*/160, /*h=*/48, "");
  UpdateMotorCurrentHzButton();
  motorCalibrationButton_ = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/96, /*w=*/160, /*h=*/48, "");
  UpdateMotorCalibrationButton();
  motorPositionButton_ = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/144, /*w=*/160, /*h=*/48, "");
  UpdateMotorPositionButton();
  motorCurrentPositionButton_ = TouchButtonManager::Get()->AddButton(/*x=*/160, /*y=*/144, /*w=*/160, /*h=*/48, "");
  UpdateMotorCurrentPositionButton();
  motorBackButton_ = TouchButtonManager::Get()->AddButton(/*x=*/0, /*y=*/192, /*w=*/320, /*h=*/48, "Back");

  // Calibration submenu buttons.
  for (int i = 0; i < 4; i++) {
    hallSensorInfoButtons_[i] = TouchButtonManager::Get()->AddButton(0, i * 45, 320, 45, "");
  }
  hallSensorBackButton_ = TouchButtonManager::Get()->AddButton(0, 180, 320, 60, "Back");

  // Status submenu buttons.
  for (int i = 0; i < kNumPlanets; i++) {
    statusInfoButtons_[i] = TouchButtonManager::Get()->AddButton(0, i * 22, 320, 22, "");
  }
  statusBackButton_ = TouchButtonManager::Get()->AddButton(0, 240 - 42, 320, 42, "Back");
  UpdateStatusMenuButton();
  UpdatePlanetButton();

  // Common UI.
  const int w = 320 / 3;
  const int h = 240 / 5;
  keypadBackButton_ = TouchButtonManager::Get()->AddButton(0, 0, w, h, "Back");
  speedDisplayButton_ = TouchButtonManager::Get()->AddButton(w, 0, 320 - w, h, "");
  speedDisplayButton_->SetCustomPaintFunction(std::bind(&OrreryLeaderUi::DrawSpeedDisplayButton, this,
                                                        std::placeholders::_1, std::placeholders::_2,
                                                        std::placeholders::_3, std::placeholders::_4));
  for (int i = 1; i <= 9; i++) {
    char label[2] = {static_cast<char>('0' + i), '\0'};
    keypadButtons_[i] = TouchButtonManager::Get()->AddButton(((i - 1) % 3) * w, ((i - 1) / 3 + 1) * h, w, h, label);
  }
  keypadButtons_[0] = TouchButtonManager::Get()->AddButton(w, 4 * h, w, h, "0");
  clearButton_ = TouchButtonManager::Get()->AddButton(0, 4 * h, w, h, "Clear");
  confirmButton_ = TouchButtonManager::Get()->AddButton(2 * w, 4 * h, 320 - 2 * w, h, "Confirm");

  const int pw = 320 / 3;
  const int ph = 240 / 4;
  for (int i = 0; i < kNumPlanets; i++) {
    Planet planet = static_cast<Planet>(static_cast<int>(Planet::Mercury) + i);
    planetSelectButtons_[i] =
        TouchButtonManager::Get()->AddButton((i % 3) * pw, (i / 3) * ph, pw, ph, GetPlanetName(planet));
  }
  planetSelectButtons_[kNumPlanets] = TouchButtonManager::Get()->AddButton(0, 3 * ph, 160, ph, "Global");
  planetBackButton_ = TouchButtonManager::Get()->AddButton(160, 3 * ph, 160, ph, "Back");

  // Scene menu buttons.
  const int sw = 320 / 3;
  const int sh = 240 / 5;
  for (int i = 0; i < 14; i++) {
    const char* label;
    if (i >= 4 && i < 13) {
      label = GetPlanetName(static_cast<Planet>(i));
    } else if (static_cast<OrreryScene>(i) == OrreryScene::MercuryRetrograde) {
      label = "MercRetro";
    } else {
      label = OrrerySceneToString(static_cast<OrreryScene>(i));
    }
    sceneSelectButtons_[i] = TouchButtonManager::Get()->AddButton((i % 3) * sw, (i / 3) * sh, sw, sh, label);
  }
  sceneBackButton_ = TouchButtonManager::Get()->AddButton(2 * sw, 4 * sh, sw, sh, "Back");

  // Initialize LED pattern mode.
  uint32_t ledPattern = OrreryLeader::Get()->GetLedPattern(currentPlanet_);
  if (ledPattern & kPlanetPatternHallSensorBit) {
    planetPatternMode_ = PlanetPatternMode::Hall;
  } else if (ledPattern & kPlanetPatternHalfBit) {
    planetPatternMode_ = PlanetPatternMode::Half;
  } else if (ledPattern == 0x00000200) {
    planetPatternMode_ = PlanetPatternMode::Green;
  } else {
    planetPatternMode_ = PlanetPatternMode::Full;
  }
  planetOffset_ = (ledPattern >> kPlanetPatternOffsetShift) & kPlanetPatternOffsetMask;
  UpdatePlanetHalfButton();
  UpdatePlanetOffsetButton();

  HideAll();
  M5.Display.fillScreen(BLACK);
  M5.Display.setBrightness(kDefaultOnBrightness);
  M5.Display.wakeup();
}

void OrreryLeaderUi::FinalSetup() { DrawMainMenu(); }

void OrreryLeaderUi::HideAll() {
  planetButton_->Hide();
  sceneButton_->Hide();
  ledMenuButton_->Hide();
  motorMenuButton_->Hide();
  calibrationMenuButton_->Hide();
  ledBrightnessButton_->Hide();
  planetHalfButton_->Hide();
  planetOffsetButton_->Hide();
  ledBackButton_->Hide();
  motorEnableButton_->Hide();
  motorDirectionButton_->Hide();
  motorPositionButton_->Hide();
  motorCurrentPositionButton_->Hide();
  motorCurrentSpeedButton_->Hide();
  motorCurrentHzButton_->Hide();
  motorCalibrationButton_->Hide();
  motorSpeedButton_->Hide();
  speedMultiplierButton_->Hide();
  motorBackButton_->Hide();
  for (int i = 0; i < 4; i++) { hallSensorInfoButtons_[i]->Hide(); }
  hallSensorBackButton_->Hide();
  for (int i = 0; i < kNumPlanets; i++) { statusInfoButtons_[i]->Hide(); }
  statusBackButton_->Hide();
  statusMenuButton_->Hide();
  keypadBackButton_->Hide();

  speedDisplayButton_->Hide();
  for (int i = 0; i <= 9; i++) { keypadButtons_[i]->Hide(); }
  clearButton_->Hide();
  confirmButton_->Hide();
  for (int i = 0; i <= kNumPlanets; i++) { planetSelectButtons_[i]->Hide(); }
  planetBackButton_->Hide();
  for (int i = 0; i < 14; i++) { sceneSelectButtons_[i]->Hide(); }
  sceneBackButton_->Hide();
}

void OrreryLeaderUi::DrawMainMenu() {
  HideAll();
  planetButton_->Draw();
  sceneButton_->Draw();
  ledMenuButton_->Draw();
  motorMenuButton_->Draw();
  calibrationMenuButton_->Draw();
  statusMenuButton_->Draw();
  speedMultiplierButton_->Draw();
  TouchButtonManager::Get()->Redraw();
}

void OrreryLeaderUi::DrawStatusMenu() {
  HideAll();
  for (int i = 0; i < kNumPlanets; i++) { statusInfoButtons_[i]->Draw(); }
  statusBackButton_->Draw();
  UpdateStatusSubmenu();
  TouchButtonManager::Get()->Redraw();
}

void OrreryLeaderUi::DrawLedMenu() {
  HideAll();
  ledBrightnessButton_->Draw();
  planetHalfButton_->Draw();
  planetOffsetButton_->Draw();
  ledBackButton_->Draw();
  TouchButtonManager::Get()->Redraw();
}

void OrreryLeaderUi::DrawMotorMenu() {
  HideAll();
  motorEnableButton_->Draw();
  motorDirectionButton_->Draw();
  motorPositionButton_->Draw();
  motorCurrentPositionButton_->Draw();
  motorCalibrationButton_->Draw();
  motorSpeedButton_->Draw();
  motorCurrentSpeedButton_->Draw();
  motorCurrentHzButton_->Draw();
  motorBackButton_->Draw();
  UpdateMotorCurrentPositionButton();
  UpdateMotorCurrentSpeedButton();
  UpdateMotorCurrentHzButton();
  UpdateMotorCalibrationButton();
  UpdateSpeedMultiplierButton();
  TouchButtonManager::Get()->Redraw();
}

void OrreryLeaderUi::DrawCalibrationMenu() {
  HideAll();
  for (int i = 0; i < 4; i++) { hallSensorInfoButtons_[i]->Draw(); }
  hallSensorBackButton_->Draw();
  UpdateHallSensorSubmenu();
  TouchButtonManager::Get()->Redraw();
}

void OrreryLeaderUi::DrawKeypad() {
  HideAll();
  keypadBackButton_->Draw();
  speedDisplayButton_->Draw();
  for (int i = 0; i <= 9; i++) { keypadButtons_[i]->Draw(); }
  clearButton_->Draw();
  confirmButton_->Draw();
  speedDisplayButton_->Draw(/*force=*/true);
  TouchButtonManager::Get()->Redraw();
}

void OrreryLeaderUi::DrawPlanetMenu() {
  HideAll();
  for (int i = 0; i <= kNumPlanets; i++) { planetSelectButtons_[i]->Draw(); }
  planetBackButton_->Draw();
  TouchButtonManager::Get()->Redraw();
}

void OrreryLeaderUi::DrawSceneMenu() {
  HideAll();
  for (int i = 0; i < 14; i++) { sceneSelectButtons_[i]->Draw(); }
  sceneBackButton_->Draw();
  TouchButtonManager::Get()->Redraw();
}

void OrreryLeaderUi::RunLoop(Milliseconds currentTime) {
  UpdateCalibrationMenuButton();
  UpdateStatusSubmenu();
  OrreryScene currentScene = OrreryLeader::Get()->GetScene();
  if (currentScene != lastScene_) {
    UpdateSceneButton();
    // Update local motor state for current planet.
    int32_t speed = OrreryLeader::Get()->GetSpeed(currentPlanet_);
    if (speed == kOrrerySpeedDisable) {
      motorEnabled_ = false;
    } else {
      motorEnabled_ = true;
      motorForward_ = (speed >= 0);
      motorMilliRpm_ = std::abs(speed);
    }
    motorEnableButton_->SetLabelText(motorEnabled_ ? "Enabled" : "Disabled");
    UpdateMotorDirectionButton();
    UpdateMotorSpeedButton();
    UpdateMotorCurrentSpeedButton();
    UpdateMotorCurrentHzButton();
    lastScene_ = currentScene;
  }
  if (motorSubmenuActive_ && !keypadActive_) {
    UpdateMotorCurrentPositionButton();
    UpdateMotorCurrentSpeedButton();
    UpdateMotorCurrentHzButton();
    UpdateMotorCalibrationButton();
  }
  M5.update();
  auto touchDetail = M5.Touch.getDetail();
  if (touchDetail.isPressed()) {
    int16_t px = touchDetail.x;
    int16_t py = touchDetail.y;
    TouchButtonManager::Get()->HandlePress(px, py);
  } else {
    TouchButtonManager::Get()->HandleIdle();
  }

  if (keypadActive_) {
    for (int i = 0; i <= 9; i++) {
      if (keypadButtons_[i]->JustReleased()) {
        keypadValue_ = keypadValue_ * 10 + i;
        keypadHasValue_ = true;
        speedDisplayButton_->Draw(/*force=*/true);
      }
    }
    if (keypadBackButton_->JustReleased()) {
      keypadActive_ = false;
      keypadHasValue_ = false;
      editingBrightness_ = false;
      editingOffset_ = false;
      editingPosition_ = false;
      if (ledSubmenuActive_) {
        DrawLedMenu();
      } else if (motorSubmenuActive_) {
        DrawMotorMenu();
      } else {
        DrawMainMenu();
      }
    }
    if (clearButton_->JustReleased()) {
      keypadValue_ = 0;
      keypadHasValue_ = false;
      speedDisplayButton_->Draw(/*force=*/true);
    }
    if (confirmButton_->JustReleased()) {
      if (editingBrightness_) {
        if (keypadValue_ > 255) { keypadValue_ = 255; }
        ledBrightness_ = keypadValue_;
        OrreryLeader::Get()->SetBrightness(currentPlanet_, ledBrightness_);
        UpdateLedBrightnessButton();
      } else if (editingOffset_) {
        if (keypadValue_ > 255) { keypadValue_ = 255; }
        planetOffset_ = keypadValue_;
        UpdatePlanetOffsetButton();
        UpdatePlanetPattern();
      } else if (editingPosition_) {
        if (keypadHasValue_) {
          targetPosition_ = keypadValue_ * 1000;
        } else {
          targetPosition_ = std::nullopt;
        }
        OrreryLeader::Get()->SetPosition(currentPlanet_, targetPosition_);
        UpdateMotorPositionButton();
      } else if (editingSpeedMultiplier_) {
        OrreryLeader::Get()->SetSpeedMultiplier(keypadValue_ / 1000.0);
        UpdateSpeedMultiplierButton();
        OrreryLeader::Get()->SetScene(OrreryLeader::Get()->GetScene());
      } else {
        motorMilliRpm_ = keypadValue_;
        SetMotorSpeed();
        UpdateMotorSpeedButton();
      }
      keypadActive_ = false;
      keypadHasValue_ = false;
      editingBrightness_ = false;
      editingOffset_ = false;
      editingPosition_ = false;
      editingSpeedMultiplier_ = false;
      if (ledSubmenuActive_) {
        DrawLedMenu();
      } else if (motorSubmenuActive_) {
        DrawMotorMenu();
      } else {
        DrawMainMenu();
      }
    }
  } else if (planetSubmenuActive_) {
    for (int i = 0; i <= kNumPlanets; i++) {
      if (planetSelectButtons_[i]->JustReleased()) {
        if (i == kNumPlanets) {
          currentPlanet_ = Planet::All;
        } else {
          currentPlanet_ = static_cast<Planet>(static_cast<int>(Planet::Mercury) + i);
        }
        UpdatePlanetButton();
        // Update UI with current speed of that planet
        int32_t speed = OrreryLeader::Get()->GetSpeed(currentPlanet_);
        if (speed == kOrrerySpeedDisable) {
          motorEnabled_ = false;
        } else {
          motorEnabled_ = true;
          motorForward_ = (speed >= 0);
          motorMilliRpm_ = std::abs(speed);
        }
        targetPosition_ = OrreryLeader::Get()->GetTargetPosition(currentPlanet_);
        motorEnableButton_->SetLabelText(motorEnabled_ ? "Enabled" : "Disabled");
        UpdateMotorDirectionButton();
        UpdateMotorPositionButton();
        UpdateMotorSpeedButton();
        UpdateMotorCurrentSpeedButton();
        UpdateMotorCurrentHzButton();
        UpdateMotorCalibrationButton();
        ledBrightness_ = OrreryLeader::Get()->GetBrightness(currentPlanet_);
        UpdateLedBrightnessButton();
        uint32_t ledPattern = OrreryLeader::Get()->GetLedPattern(currentPlanet_);
        if (ledPattern & kPlanetPatternHallSensorBit) {
          planetPatternMode_ = PlanetPatternMode::Hall;
        } else if (ledPattern & kPlanetPatternHalfBit) {
          planetPatternMode_ = PlanetPatternMode::Half;
        } else if (ledPattern == 0x00000200) {
          planetPatternMode_ = PlanetPatternMode::Green;
        } else {
          planetPatternMode_ = PlanetPatternMode::Full;
        }
        planetOffset_ = (ledPattern >> kPlanetPatternOffsetShift) & kPlanetPatternOffsetMask;
        UpdatePlanetHalfButton();
        UpdatePlanetOffsetButton();
        planetSubmenuActive_ = false;
        DrawMainMenu();
      }
    }
    if (planetBackButton_->JustReleased()) {
      planetSubmenuActive_ = false;
      DrawMainMenu();
    }
  } else if (sceneSubmenuActive_) {
    for (int i = 0; i < 14; i++) {
      if (sceneSelectButtons_[i]->JustReleased()) {
        OrreryLeader::Get()->SetScene(static_cast<OrreryScene>(i));
        UpdateSceneButton();
        // Update local motor state for current planet.
        int32_t speed = OrreryLeader::Get()->GetSpeed(currentPlanet_);
        if (speed == kOrrerySpeedDisable) {
          motorEnabled_ = false;
        } else {
          motorEnabled_ = true;
          motorForward_ = (speed >= 0);
          motorMilliRpm_ = std::abs(speed);
        }
        motorEnableButton_->SetLabelText(motorEnabled_ ? "Enabled" : "Disabled");
        UpdateMotorDirectionButton();
        UpdateMotorSpeedButton();
        sceneSubmenuActive_ = false;
        DrawMainMenu();
      }
    }
    if (sceneBackButton_->JustReleased()) {
      sceneSubmenuActive_ = false;
      DrawMainMenu();
    }
  } else if (ledSubmenuActive_) {
    if (ledBrightnessButton_->JustReleased()) {
      keypadActive_ = true;
      keypadHasValue_ = false;
      editingBrightness_ = true;
      editingOffset_ = false;
      keypadValue_ = 0;
      DrawKeypad();
    }
    if (planetHalfButton_->JustReleased()) {
      if (planetPatternMode_ == PlanetPatternMode::Full) {
        planetPatternMode_ = PlanetPatternMode::Half;
      } else if (planetPatternMode_ == PlanetPatternMode::Half) {
        planetPatternMode_ = PlanetPatternMode::Hall;
      } else if (planetPatternMode_ == PlanetPatternMode::Hall) {
        planetPatternMode_ = PlanetPatternMode::Green;
      } else {
        planetPatternMode_ = PlanetPatternMode::Full;
      }
      UpdatePlanetHalfButton();
      UpdatePlanetPattern();
    }
    if (planetOffsetButton_->JustReleased()) {
      keypadActive_ = true;
      keypadHasValue_ = false;
      editingBrightness_ = false;
      editingOffset_ = true;
      keypadValue_ = 0;
      DrawKeypad();
    }
    if (ledBackButton_->JustReleased()) {
      ledSubmenuActive_ = false;
      DrawMainMenu();
    }
  } else if (motorSubmenuActive_) {
    if (motorEnableButton_->JustReleased()) {
      motorEnabled_ = !motorEnabled_;
      motorEnableButton_->SetLabelText(motorEnabled_ ? "Enabled" : "Disabled");
      SetMotorSpeed();
    }
    if (motorDirectionButton_->JustReleased()) {
      motorForward_ = !motorForward_;
      UpdateMotorDirectionButton();
      SetMotorSpeed();
    }
    if (motorPositionButton_->JustReleased()) {
      keypadActive_ = true;
      keypadHasValue_ = false;
      editingBrightness_ = false;
      editingOffset_ = false;
      editingPosition_ = true;
      keypadValue_ = 0;
      DrawKeypad();
    }
    if (motorSpeedButton_->JustReleased()) {
      keypadActive_ = true;
      keypadHasValue_ = false;
      editingBrightness_ = false;
      editingOffset_ = false;
      editingPosition_ = false;
      editingSpeedMultiplier_ = false;
      keypadValue_ = 0;
      DrawKeypad();
    }
    if (motorBackButton_->JustReleased()) {
      motorSubmenuActive_ = false;
      DrawMainMenu();
    }
  } else if (hallSensorSubmenuActive_) {
    if (hallSensorBackButton_->JustReleased()) {
      hallSensorSubmenuActive_ = false;
      DrawMainMenu();
    }
  } else if (statusSubmenuActive_) {
    if (statusBackButton_->JustReleased()) {
      statusSubmenuActive_ = false;
      DrawMainMenu();
    }
  } else {
    if (planetButton_->JustReleased()) {
      planetSubmenuActive_ = true;
      DrawPlanetMenu();
    }
    if (sceneButton_->JustReleased()) {
      sceneSubmenuActive_ = true;
      DrawSceneMenu();
    }
    if (ledMenuButton_->JustReleased()) {
      ledSubmenuActive_ = true;
      DrawLedMenu();
    }
    if (motorMenuButton_->JustReleased()) {
      motorSubmenuActive_ = true;
      DrawMotorMenu();
    }
    if (calibrationMenuButton_->JustReleased()) {
      hallSensorSubmenuActive_ = true;
      DrawCalibrationMenu();
    }
    if (statusMenuButton_->JustReleased()) {
      statusSubmenuActive_ = true;
      DrawStatusMenu();
    }
    if (speedMultiplierButton_->JustReleased()) {
      keypadActive_ = true;
      keypadHasValue_ = false;
      editingBrightness_ = false;
      editingOffset_ = false;
      editingPosition_ = false;
      editingSpeedMultiplier_ = true;
      keypadValue_ = 0;
      DrawKeypad();
    }
  }
  TouchButtonManager::Get()->MaybePaint();
}

void OrreryLeaderUi::SetMotorSpeed() {
  if (motorEnabled_) {
    int32_t speed = motorMilliRpm_;
    if (!motorForward_) { speed = -speed; }
    OrreryLeader::Get()->SetSpeed(currentPlanet_, speed);
  } else {
    OrreryLeader::Get()->SetSpeed(currentPlanet_, kOrrerySpeedDisable);
  }
}

void OrreryLeaderUi::UpdateMotorDirectionButton() {
  motorDirectionButton_->SetLabelText(motorForward_ ? "Forward" : "Reverse");
}

void OrreryLeaderUi::UpdateMotorSpeedButton() {
  char label[32];
  snprintf(label, sizeof(label), "Set: %.1f RPM", motorMilliRpm_ / 1000.0f);
  motorSpeedButton_->SetLabelText(label);
}

void OrreryLeaderUi::UpdateSpeedMultiplierButton() {
  char label[32];
  snprintf(label, sizeof(label), "Mult: %.3f",
           OrreryLeader::Get()->GetScene() == OrreryScene::Realistic ||
                   OrreryLeader::Get()->GetScene() == OrreryScene::Silly ||
                   OrreryLeader::Get()->GetScene() == OrreryScene::MercuryRetrograde
               ? OrreryLeader::Get()->GetSpeedMultiplier()
               : 1.0);
  speedMultiplierButton_->SetLabelText(label);
}

void OrreryLeaderUi::UpdateMotorCurrentSpeedButton() {
  char label[32];
  std::optional<int32_t> reportedSpeed = OrreryLeader::Get()->GetReportedSpeed(currentPlanet_);
  if (reportedSpeed.has_value()) {
    std::optional<uint32_t> calibration = OrreryLeader::Get()->GetCalibration(currentPlanet_);
    if (calibration.has_value() && *calibration > 0) {
      float rpm = (*reportedSpeed * 60.0f) / *calibration;
      snprintf(label, sizeof(label), "Speed: %.1f RPM", rpm);
    } else {
      snprintf(label, sizeof(label), "Speed: Unknown");
    }
  } else {
    snprintf(label, sizeof(label), "Speed: Unknown");
  }
  motorCurrentSpeedButton_->SetLabelText(label);
}

void OrreryLeaderUi::UpdateMotorCurrentHzButton() {
  char label[32];
  std::optional<int32_t> reportedSpeed = OrreryLeader::Get()->GetReportedSpeed(currentPlanet_);
  if (reportedSpeed.has_value()) {
    snprintf(label, sizeof(label), "Speed: %ld Hz", (long)*reportedSpeed);
  } else {
    snprintf(label, sizeof(label), "Speed: Unknown");
  }
  motorCurrentHzButton_->SetLabelText(label);
}

void OrreryLeaderUi::UpdateSceneButton() {
  char label[32];
  snprintf(label, sizeof(label), "Scene: %s", OrrerySceneToString(OrreryLeader::Get()->GetScene()));
  sceneButton_->SetLabelText(label);
}

void OrreryLeaderUi::UpdateMotorPositionButton() {
  char label[32];
  if (targetPosition_.has_value()) {
    snprintf(label, sizeof(label), "Set: %lu deg", (unsigned long)(*targetPosition_ / 1000));
  } else {
    snprintf(label, sizeof(label), "Set: unset");
  }
  motorPositionButton_->SetLabelText(label);
}

void OrreryLeaderUi::UpdateMotorCurrentPositionButton() {
  char label[32];
  std::optional<uint32_t> currentPos = OrreryLeader::Get()->GetPosition(currentPlanet_);
  if (currentPos.has_value()) {
    snprintf(label, sizeof(label), "Pos: %lu deg", (unsigned long)(*currentPos / 1000));
  } else {
    snprintf(label, sizeof(label), "Pos: Unknown");
  }
  motorCurrentPositionButton_->SetLabelText(label);
}

void OrreryLeaderUi::UpdateMotorCalibrationButton() {
  char label[32];
  std::optional<uint32_t> calibration = OrreryLeader::Get()->GetCalibration(currentPlanet_);
  if (calibration.has_value()) {
    snprintf(label, sizeof(label), "Calib: %lu", (unsigned long)*calibration);
  } else {
    snprintf(label, sizeof(label), "Calib: Unknown");
  }
  motorCalibrationButton_->SetLabelText(label);
}

void OrreryLeaderUi::UpdateLedBrightnessButton() {
  char label[32];
  snprintf(label, sizeof(label), "Brightness: %u", ledBrightness_);
  ledBrightnessButton_->SetLabelText(label);
}

void OrreryLeaderUi::UpdatePlanetButton() {
  char label[32];
  snprintf(label, sizeof(label), "Planet: %s", GetPlanetName(currentPlanet_));
  planetButton_->SetLabelText(label);
  UpdateStatusMenuButton();
}

void OrreryLeaderUi::UpdatePlanetHalfButton() {
  const char* modeName = "Full";
  if (planetPatternMode_ == PlanetPatternMode::Half) {
    modeName = "Half";
  } else if (planetPatternMode_ == PlanetPatternMode::Hall) {
    modeName = "Hall Sensor";
  } else if (planetPatternMode_ == PlanetPatternMode::Green) {
    modeName = "Green";
  }
  char label[32];
  snprintf(label, sizeof(label), "Pattern: %s", modeName);
  planetHalfButton_->SetLabelText(label);
}

void OrreryLeaderUi::UpdatePlanetOffsetButton() {
  char label[32];
  snprintf(label, sizeof(label), "Offset: %u", planetOffset_);
  planetOffsetButton_->SetLabelText(label);
}

void OrreryLeaderUi::UpdatePlanetPattern() {
  uint32_t ledPattern;
  if (planetPatternMode_ == PlanetPatternMode::Green) {
    ledPattern = 0x00000200;
  } else {
    ledPattern = kPlanetPattern | (planetOffset_ << kPlanetPatternOffsetShift);
    if (planetPatternMode_ == PlanetPatternMode::Hall) {
      ledPattern |= kPlanetPatternHallSensorBit;
    } else if (planetPatternMode_ == PlanetPatternMode::Half) {
      ledPattern |= kPlanetPatternHalfBit;
    }
  }
  OrreryLeader::Get()->SetLedPattern(currentPlanet_, ledPattern);
}

void OrreryLeaderUi::UpdateLedMenuButton() { ledMenuButton_->SetLabelText("LED"); }

void OrreryLeaderUi::UpdateMotorMenuButton() { motorMenuButton_->SetLabelText("Motor"); }

void OrreryLeaderUi::UpdateStatusMenuButton() { statusMenuButton_->SetLabelText("Status"); }

void OrreryLeaderUi::UpdateStatusSubmenu() {
  if (!statusSubmenuActive_) { return; }
  for (int i = 0; i < kNumPlanets; i++) {
    Planet planet = static_cast<Planet>(static_cast<int>(Planet::Mercury) + i);
    std::optional<Milliseconds> lastHeard = OrreryLeader::Get()->GetLastHeardTime(planet);
    std::optional<Milliseconds> maxRtt = OrreryLeader::Get()->GetMaxRtt(planet);
    char label[64];
    if (lastHeard.has_value()) {
      if (maxRtt.has_value()) {
        snprintf(label, sizeof(label), "%s: %llds ago, max %lldms", GetPlanetName(planet),
                 static_cast<long long>((timeMillis() - *lastHeard) / 1000), static_cast<long long>(*maxRtt));
      } else {
        snprintf(label, sizeof(label), "%s: %llds ago", GetPlanetName(planet),
                 static_cast<long long>((timeMillis() - *lastHeard) / 1000));
      }
    } else {
      snprintf(label, sizeof(label), "%s: Never heard", GetPlanetName(planet));
    }
    statusInfoButtons_[i]->SetLabelText(label);
  }
}

void OrreryLeaderUi::UpdateCalibrationMenuButton() {
  std::optional<Milliseconds> lastOpened = OrreryLeader::Get()->GetTimeHallSensorLastOpened(currentPlanet_);
  std::optional<Milliseconds> lastClosed = OrreryLeader::Get()->GetTimeHallSensorLastClosed(currentPlanet_);
  char label[32];
  if (lastOpened.has_value() || lastClosed.has_value()) {
    bool isClosed = false;
    if (lastOpened.has_value() && lastClosed.has_value()) {
      isClosed = (*lastClosed > *lastOpened);
    } else if (lastClosed.has_value()) {
      isClosed = true;
    }
    snprintf(label, sizeof(label), "HS %s", isClosed ? "Closed" : "Open");
  } else {
    snprintf(label, sizeof(label), "HS");
  }
  calibrationMenuButton_->SetLabelText(label);
}

void OrreryLeaderUi::UpdateHallSensorSubmenu() {
  if (!hallSensorSubmenuActive_) { return; }
  std::optional<Milliseconds> lastOpened = OrreryLeader::Get()->GetTimeHallSensorLastOpened(currentPlanet_);
  std::optional<Milliseconds> lastClosed = OrreryLeader::Get()->GetTimeHallSensorLastClosed(currentPlanet_);
  std::optional<Milliseconds> lastOpenDuration = OrreryLeader::Get()->GetLastOpenDuration(currentPlanet_);
  std::optional<Milliseconds> lastClosedDuration = OrreryLeader::Get()->GetLastClosedDuration(currentPlanet_);

  char label[64];
  if (lastOpened.has_value()) {
    snprintf(label, sizeof(label), "Last Opened: %llds ago",
             static_cast<long long>((timeMillis() - *lastOpened) / 1000));
  } else {
    snprintf(label, sizeof(label), "Last Opened: Unknown");
  }
  hallSensorInfoButtons_[0]->SetLabelText(label);

  if (lastClosed.has_value()) {
    snprintf(label, sizeof(label), "Last Closed: %llds ago",
             static_cast<long long>((timeMillis() - *lastClosed) / 1000));
  } else {
    snprintf(label, sizeof(label), "Last Closed: Unknown");
  }
  hallSensorInfoButtons_[1]->SetLabelText(label);

  if (lastOpenDuration.has_value()) {
    snprintf(label, sizeof(label), "Last Open Dur: %lldms", static_cast<long long>(*lastOpenDuration));
  } else {
    snprintf(label, sizeof(label), "Last Open Dur: Unknown");
  }
  hallSensorInfoButtons_[2]->SetLabelText(label);

  if (lastClosedDuration.has_value()) {
    snprintf(label, sizeof(label), "Last Closed Dur: %lldms", static_cast<long long>(*lastClosedDuration));
  } else {
    snprintf(label, sizeof(label), "Last Closed Dur: Unknown");
  }
  hallSensorInfoButtons_[3]->SetLabelText(label);
}

void OrreryLeaderUi::DrawSpeedDisplayButton(TouchButton* button, int /*outline*/, int fill, int textColor) {
  button->PaintRectangle(fill, /*outline=*/fill);  // Skip outline.
  char label[32];
  const char* valueStr = keypadHasValue_ ? "" : "_";
  if (editingBrightness_) {
    if (keypadHasValue_) {
      snprintf(label, sizeof(label), "Brightness: %" PRId32, keypadValue_);
    } else {
      snprintf(label, sizeof(label), "Brightness: _");
    }
  } else if (editingOffset_) {
    if (keypadHasValue_) {
      snprintf(label, sizeof(label), "Offset: %" PRId32, keypadValue_);
    } else {
      snprintf(label, sizeof(label), "Offset: _");
    }
  } else if (editingPosition_) {
    if (keypadHasValue_) {
      snprintf(label, sizeof(label), "Pos: %" PRId32, keypadValue_);
    } else {
      snprintf(label, sizeof(label), "Pos: _");
    }
  } else if (editingSpeedMultiplier_) {
    if (keypadHasValue_) {
      snprintf(label, sizeof(label), "Mult: %.3f", keypadValue_ / 1000.0);
    } else {
      snprintf(label, sizeof(label), "Mult: _");
    }
  } else {
    if (keypadHasValue_) {
      snprintf(label, sizeof(label), "Set: %.1f RPM", keypadValue_ / 1000.0);
    } else {
      snprintf(label, sizeof(label), "Set: _ RPM");
    }
  }
  button->PaintText(textColor, fill, label);
}

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_LEADER) && ESP32 && (CORE2AWS || CORES3)
