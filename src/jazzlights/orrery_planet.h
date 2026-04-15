#ifndef JL_ORRERY_PLANET_H
#define JL_ORRERY_PLANET_H

#include "jazzlights/config.h"

#if JL_IS_CONFIG(ORRERY_PLANET)

#include "jazzlights/network/max485_bus.h"
#include "jazzlights/ui/gpio_button.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

class Player;

class OrreryPlanet : public GpioSwitch::SwitchInterface {
 public:
  static OrreryPlanet* Get();
  void Setup(Player& player);
  void RunLoop(Milliseconds currentTime);

  // From GpioSwitch::SwitchInterface.
  void StateChanged(uint8_t pin, bool isClosed) override;

 private:
  OrreryPlanet();

  BusId ComputeBusId() const;

  Player* player_ = nullptr;
  GpioSwitch switch0_;
  GpioSwitch switch1_;
  GpioSwitch switch2_;
  BusId busId_;
  Max485BusFollower max485BusFollower_;
};

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_PLANET)

#endif  // JL_ORRERY_PLANET_H
