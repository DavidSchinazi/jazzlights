#include "esphome/components/jazzlights/jazzlights_light.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::jazzlights_esphome {

static const char* const TAG = "jazzlights.light";

namespace {
uint8_t ToByte(float value) { return static_cast<uint8_t>(std::lround(clamp(value, 0.0f, 1.0f) * 255.0f)); }
}  // namespace

light::LightTraits JazzLightsLight::get_traits() {
  light::LightTraits traits;
  traits.set_supported_color_modes({light::ColorMode::RGB});
  return traits;
}

void JazzLightsLight::setup_state(light::LightState* state) {
  this->state_ = state;
  this->parent_->player()->SetStatusWatcher(this);
}

void JazzLightsLight::write_state(light::LightState* state) {
  ::jazzlights::Player* player = this->parent_->player();
  this->writing_ = true;
  const light::LightColorValues& values = state->current_values;
  const bool effect_active = state->get_current_effect_index() != 0;
  if (!values.is_on()) {
    // Reset to default parameters when turned off, like the WebSocket server does.
    player->SetEnabled(false);
    player->SetBrightness(255);
    player->DisableColorOverride();
    this->pattern_mode_ = true;
    this->was_on_ = false;
  } else {
    // The target color, as opposed to the current one which changes during transitions.
    const light::LightColorValues& target = state->remote_values;
    const CRGB color(ToByte(target.get_red()), ToByte(target.get_green()), ToByte(target.get_blue()));
    const bool color_changed = this->last_color_.has_value() && *this->last_color_ != color;
    if (color_changed) {
      this->pattern_mode_ = false;
    } else if (effect_active && !this->effect_was_active_) {
      this->pattern_mode_ = true;
    } else if (!effect_active && this->effect_was_active_) {
      this->pattern_mode_ = false;
    } else if (!this->was_on_) {
      // Turning on without picking a color goes back to patterns, like the WebSocket server does.
      this->pattern_mode_ = true;
    }
    player->SetEnabled(true);
    player->SetBrightness(ToByte(values.get_brightness()));
    if (this->pattern_mode_) {
      player->DisableColorOverride();
    } else {
      player->EnableColorOverride(
          CRGB(ToByte(values.get_red()), ToByte(values.get_green()), ToByte(values.get_blue())));
    }
    this->was_on_ = true;
    this->effect_needs_sync_ = (this->pattern_mode_ != effect_active);
  }
  this->last_color_ = CRGB(ToByte(state->remote_values.get_red()), ToByte(state->remote_values.get_green()),
                           ToByte(state->remote_values.get_blue()));
  this->effect_was_active_ = effect_active;
  this->writing_ = false;
}

void JazzLightsLight::OnStatus() {
  if (this->writing_) { return; }
  this->player_status_changed_ = true;
}

void JazzLightsLight::loop() {
  if (this->state_ == nullptr) { return; }
  if (this->player_status_changed_) {
    this->player_status_changed_ = false;
    this->effect_needs_sync_ = false;
    this->publish_player_state_();
  } else if (this->effect_needs_sync_) {
    this->effect_needs_sync_ = false;
    ESP_LOGD(TAG, "Setting effect to match %s mode", this->pattern_mode_ ? "pattern" : "color");
    this->state_->make_call().set_effect(uint32_t{this->pattern_mode_ ? 1u : 0u}).perform();
  }
}

void JazzLightsLight::publish_player_state_() {
  ::jazzlights::Player* player = this->parent_->player();
  light::LightCall call = this->state_->make_call();
  if (!player->enabled()) {
    if (!this->state_->remote_values.is_on()) { return; }
    ESP_LOGD(TAG, "Publishing off from player");
    call.set_state(false);
  } else {
    const std::optional<CRGB>& colorOverride = player->colorOverride();
    ESP_LOGD(TAG, "Publishing on brightness=%u %s from player", player->brightness(),
             colorOverride ? "color" : "patterns");
    call.set_state(true);
    call.set_brightness(player->brightness() / 255.0f);
    if (colorOverride) {
      call.set_rgb(colorOverride->r / 255.0f, colorOverride->g / 255.0f, colorOverride->b / 255.0f);
      call.set_effect(uint32_t{0});
    } else {
      call.set_effect(uint32_t{1});
    }
  }
  call.perform();
}

}  // namespace esphome::jazzlights_esphome
