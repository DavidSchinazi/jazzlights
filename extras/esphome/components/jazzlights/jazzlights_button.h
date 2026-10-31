#pragma once

#include "esphome/components/button/button.h"
#include "esphome/components/jazzlights/jazzlights_component.h"
#include "esphome/core/helpers.h"

namespace esphome::jazzlights_esphome {

// Switches the clouds to the next JazzLights pattern.
class JazzLightsNextButton : public button::Button, public Parented<JazzLightsComponent> {
 protected:
  void press_action() override { this->parent_->player()->CloudNext(); }
};

}  // namespace esphome::jazzlights_esphome
