#pragma once

#include <optional>

#include "esphome/components/jazzlights/jazzlights_component.h"
#include "esphome/components/light/light_effect.h"
#include "esphome/components/light/light_output.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "jazzlights/render/player.h"

namespace esphome::jazzlights_esphome {

// Exposes the clouds as an RGB light. The light either renders JazzLights patterns, which is shown as the "Clouds"
// effect, or a solid color, which is shown as no effect. Like the WebSocket server, turning the light off hands the
// clouds back to their default patterns at full brightness.
class JazzLightsLight : public light::LightOutput,
                        public Component,
                        public Parented<JazzLightsComponent>,
                        public ::jazzlights::Player::StatusWatcher {
 public:
  light::LightTraits get_traits() override;
  void setup_state(light::LightState* state) override;
  void write_state(light::LightState* state) override;
  void loop() override;

  // From ::jazzlights::Player::StatusWatcher.
  void OnStatus() override;

 protected:
  void publish_player_state_();

  light::LightState* state_ = nullptr;
  // Whether we render JazzLights patterns, as opposed to a solid color.
  bool pattern_mode_ = true;
  bool was_on_ = false;
  bool effect_was_active_ = false;
  std::optional<CRGB> last_color_;
  // Set while we're writing to the player, to ignore the status updates that causes.
  bool writing_ = false;
  // Set when the player's status changed from elsewhere (e.g., the button), so we publish it to the light.
  bool player_status_changed_ = false;
  // Set when the light's effect doesn't match pattern_mode_.
  bool effect_needs_sync_ = false;
};

// No-op effect, selecting it switches the light to JazzLights patterns. It is only allowed on JazzLightsLight.
class JazzLightsCloudsEffect : public light::LightEffect {
 public:
  explicit JazzLightsCloudsEffect(const char* name) : LightEffect(name) {}
  void apply() override {}
};

}  // namespace esphome::jazzlights_esphome
