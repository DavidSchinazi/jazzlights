#ifndef JL_ORRERY_PLANET_H
#define JL_ORRERY_PLANET_H

#include "jazzlights/config.h"

#if JL_IS_CONFIG(ORRERY_PLANET)

#include "jazzlights/network/max485_bus.h"
#include "jazzlights/ui/gpio_button.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

class OrreryPlanet : public GpioSwitch::SwitchInterface {
 public:
  static OrreryPlanet* Get();
  void RunLoop(Milliseconds currentTime);

  // Returns the BusId of the planet we are based on the switches.
  BusId GetBusId() const;

  // From GpioSwitch::SwitchInterface.
  void StateChanged(uint8_t pin, bool isClosed) override;

 private:
  OrreryPlanet();

  void UpdateBusId();

  GpioSwitch switch0_;
  GpioSwitch switch1_;
  GpioSwitch switch2_;
  BusId busId_;
  Max485BusHandler max485BusHandler_;
};

}  // namespace jazzlights

#endif  // JL_IS_CONFIG(ORRERY_PLANET)

#endif  // JL_ORRERY_PLANET_H
