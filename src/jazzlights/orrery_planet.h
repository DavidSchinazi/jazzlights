#ifndef JL_ORRERY_PLANET_H
#define JL_ORRERY_PLANET_H

#include "jazzlights/config.h"

#if JL_MAX485_BUS

#include "jazzlights/network/max485_bus.h"
#include "jazzlights/ui/gpio_button.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

class OrreryPlanet {
 public:
  static OrreryPlanet* Get();
  void RunLoop(Milliseconds currentTime);

  // Returns the BusId of the planet we are based on the switches.
  BusId GetBusId() const;

 private:
  OrreryPlanet();

  GpioSwitch switch0_;
  GpioSwitch switch1_;
  GpioSwitch switch2_;
};

}  // namespace jazzlights

#endif  // JL_MAX485_BUS

#endif  // JL_ORRERY_PLANET_H
