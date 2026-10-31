#pragma once

#include <cmath>
#include <cstdint>

#include "esphome/components/jazzlights/jazzlights_component.h"
#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

namespace esphome::jazzlights_esphome {

// Caps the brightness of the clouds, as a percentage, regardless of the light's brightness or whether the light is on.
// The value is saved across reboots.
class JazzLightsMaxBrightnessNumber : public number::Number, public Component, public Parented<JazzLightsComponent> {
 public:
  void setup() override {
    this->pref_ = this->make_entity_preference<float>();
    float value;
    if (!this->pref_.load(&value)) { value = 100; }
    this->apply_(value);
  }

 protected:
  void control(float value) override {
    this->apply_(value);
    this->pref_.save(&value);
  }

  void apply_(float value) {
    this->parent_->player()->SetMaxBrightness(static_cast<uint8_t>(std::lround(value * 255 / 100)));
    this->publish_state(value);
  }

  ESPPreferenceObject pref_;
};

}  // namespace esphome::jazzlights_esphome
