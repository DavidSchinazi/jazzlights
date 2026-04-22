#ifndef JL_ORRERY_LEADER_H
#define JL_ORRERY_LEADER_H

#include "jazzlights/config.h"

#if JL_IS_CONFIG(ORRERY_LEADER)

#include <cstdint>
#include <unordered_map>

#include "jazzlights/network/max485_bus.h"
#include "jazzlights/orrery_common.h"
#include "jazzlights/ui/gpio_button.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

enum class OrreryScene {
  Paused,
  Realistic,
};

const char* OrrerySceneToString(OrreryScene scene);

class OrreryLeader : public GpioSwitchInterface {
 public:
  static OrreryLeader* Get();
  void SetScene(OrreryScene scene);
  OrreryScene GetScene() const { return scene_; }
  void SetSpeed(Planet planet, int32_t speed);
  int32_t GetSpeed(Planet planet) const;
  void SetPosition(Planet planet, std::optional<uint32_t> position);
  std::optional<uint32_t> GetPosition(Planet planet) const;
  std::optional<uint32_t> GetCalibration(Planet planet) const;
  void SetBrightness(Planet planet, uint8_t brightness);
  uint8_t GetBrightness(Planet planet) const;
  void SetLedPattern(Planet planet, uint32_t ledPattern);
  uint32_t GetLedPattern(Planet planet) const;
  std::optional<Milliseconds> GetTimeHallSensorLastOpened(Planet planet) const;
  std::optional<Milliseconds> GetTimeHallSensorLastClosed(Planet planet) const;
  std::optional<Milliseconds> GetLastOpenDuration(Planet planet) const;
  std::optional<Milliseconds> GetLastClosedDuration(Planet planet) const;

  // From GpioSwitchInterface.
  void StateChanged(uint8_t pin, bool isClosed) override;

  std::optional<Milliseconds> GetLastHeardTime(Planet planet) const;
  std::optional<Milliseconds> GetMaxRtt(Planet planet) const;

  void RunLoop(Milliseconds currentTime);

 private:
  OrreryLeader();
  void SendMessage(Planet planet);
  void HandleSwitch1(bool isClosed);
  void HandleSwitch4(bool isClosed);
  OrreryScene scene_;
  const uint32_t bootId_;
  uint32_t nextSequenceNumber_ = 0;
  std::unordered_map<Planet, OrreryMessage> messages_;
  std::unordered_map<Planet, OrreryMessage> responses_;
  std::unordered_map<Planet, Milliseconds> lastHeardTime_;
  std::unordered_map<Planet, Milliseconds> maxRtt_;
  Max485BusLeader max485BusLeader_;
  GpioSwitchLow switch1_;
  GpioSwitchLow switch3_;
  GpioSwitchLow switch4_;
};

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_LEADER)

#endif  // JL_ORRERY_LEADER_H
