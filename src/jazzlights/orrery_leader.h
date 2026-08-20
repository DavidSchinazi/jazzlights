#ifndef JL_ORRERY_LEADER_H
#define JL_ORRERY_LEADER_H

#include "jazzlights/config.h"

#if JL_IS_CONFIG(ORRERY_LEADER)

#include <cstdint>
#include <unordered_map>

#include "jazzlights/network/max485_bus.h"
#include "jazzlights/orrery_common.h"
#include "jazzlights/player.h"
#include "jazzlights/ui/gpio_button.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

class OrreryLeader : public GpioSwitchInterface,
                     public Player::OverriddenPatternWatcher,
                     public Player::OrrerySceneIdWatcher {
 public:
  static OrreryLeader* Get();
  void Setup(Player& player);
  void SetScene(OrreryScene scene);
  OrreryScene GetScene() const { return scene_; }
  void SetSpeed(Planet planet, int32_t speed);
  int32_t GetSpeed(Planet planet) const;
  std::optional<int32_t> GetReportedSpeed(Planet planet) const;
  void SetPosition(Planet planet, std::optional<uint32_t> position);
  std::optional<uint32_t> GetPosition(Planet planet) const;
  std::optional<uint32_t> GetTargetPosition(Planet planet) const;
  std::optional<uint32_t> GetCalibration(Planet planet) const;
  void SetBrightness(Planet planet, uint8_t brightness);
  uint8_t GetBrightness(Planet planet) const;
  void SetSpeedMultiplier(double speedMultiplier) { speedMultiplier_ = speedMultiplier; }
  double GetSpeedMultiplier() const { return speedMultiplier_; }
  void SetLedPattern(Planet planet, uint32_t ledPattern);
  uint32_t GetLedPattern(Planet planet) const;
  std::optional<Milliseconds> GetTimeHallSensorLastOpened(Planet planet) const;
  std::optional<Milliseconds> GetTimeHallSensorLastClosed(Planet planet) const;
  std::optional<Milliseconds> GetLastOpenDuration(Planet planet) const;
  std::optional<Milliseconds> GetLastClosedDuration(Planet planet) const;

  // From GpioSwitchInterface.
  void StateChanged(uint8_t pin, bool isClosed) override;

  // From Player::OverriddenPatternWatcher.
  void OnOverriddenPattern(std::optional<PatternBits> pattern) override;
  // From Player::OrrerySceneIdWatcher.
  void OnOrrerySceneId(std::optional<OrrerySceneId> orrerySceneId) override;

  std::optional<Milliseconds> GetLastHeardTime(Planet planet) const;
  std::optional<Milliseconds> GetMaxRtt(Planet planet) const;

  void RunLoop(Milliseconds currentTime);

 private:
  OrreryLeader();
  void SendMessage(Planet planet);
  void SendBroadcastMessage(const OrreryMessage& msg);
  void HandleSwitch1(bool isClosed);
  void HandleSwitch3(bool isClosed);
  void HandleSwitch4(bool isClosed);
  OrreryScene scene_;
  double speedMultiplier_ = 3.0;
  Milliseconds sceneStartTime_;
  Milliseconds lastRandomSceneTime_;
  Milliseconds nextRandomSceneDuration_;
  const uint32_t bootId_;
  uint32_t nextSequenceNumber_ = 0;
  std::unordered_map<Planet, OrreryMessage> messages_;
  std::unordered_map<Planet, OrreryMessage> responses_;
  std::unordered_map<Planet, Milliseconds> lastHeardTime_;
  std::unordered_map<Planet, Milliseconds> maxRtt_;
  std::unordered_map<Planet, bool> hasPassedHalfway_;
  Max485BusLeader max485BusLeader_;
  GpioSwitchLow switch1_;
  GpioSwitchLow switch3_;
  GpioSwitchLow switch4_;
  bool waitingForAlignment_ = false;
  Player* player_ = nullptr;
  std::optional<PatternBits> patternOverride_;
  std::unordered_map<Planet, PatternBits> patternBeforeOverride_;
  Milliseconds receivedSceneActionTime_ = -1;
  OrrerySceneId receivedOrrerySceneId_ = static_cast<OrrerySceneId>(OrreryScene::kInvalidScene);
};

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_LEADER)

#endif  // JL_ORRERY_LEADER_H
